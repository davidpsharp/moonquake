// linktest: run two copies of a GBA ROM linked by an emulated link cable (libmgba), drive
// them with scripted and random input, and check the two games stay in step.
//
// Every frame the game logic finishes (the game writes universalTimer to frameDone) each
// emulator hashes the game state symbols given with --sym. At the end the hashes for each
// universalTimer value are compared between the two Gameboys; any difference is a desync.
//
// See tools/linktest/run.sh for usage.

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/lockstep.h>
#include <mgba/core/log.h>
#include <mgba/core/thread.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/sio/lockstep.h>
#include <mgba-util/vfs.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_GBAS 2
#define MAX_SYMS 32
#define MAX_SCRIPT 64
#define MAX_SHOTS 64

#define KEY_A       (1 << 0)
#define KEY_B       (1 << 1)
#define KEY_START   (1 << 3)
#define KEY_RIGHT   (1 << 4)
#define KEY_LEFT    (1 << 5)
#define KEY_UP      (1 << 6)
#define KEY_DOWN    (1 << 7)

struct Sym {
	char name[48];
	uint32_t addr;
	uint32_t size;
};

struct ScriptStep {
	int frame;
	int keys;
	int len;
};

struct Gba {
	int id;
	struct mCore* core;
	struct mCoreThread thread;
	struct GBASIOLockstepNode node;
	color_t* video;

	// lockstep glue, as in mGBA's Qt MultiplayerController
	int awake;
	unsigned waitMask;
	int32_t cyclesPosted;

	// scripted input before the game starts, random play after
	struct ScriptStep script[MAX_SCRIPT];
	int scriptLen;
	uint32_t rng;
	int botKeys;
	int botHold;
	int pauseAt;            // game frame to press start at, 0 for never
	int pauseStep;
	int resetAt;            // video frame to reset this Gameboy at (as if switched off), 0 for never

	int frames;
	int gameFrames;         // frames during the game proper
	int firstGameFrame;     // video frames when the first and last game frames finished
	int lastGameFrame;
	int gameFramesDone;
	int gaps[4];            // video frames between game frames: 1, 2, 3, more (banners etc)
	int lastTimer;
	int lastFrameDone;
	uint32_t* hashes;       // per universalTimer, [timer * (numSyms + 1)]
	uint8_t* seen;
	int ended;              // matchOver seen
	int shots[MAX_SHOTS];
	int numShots;
};

static struct GBASIOLockstep lockstep;
static struct mLockstep* ls;
static struct Gba gba[NUM_GBAS];
static struct Sym syms[MAX_SYMS];
static int numSyms;
static uint32_t symFrameDone, symTimer, symNumPlayers, symMatchOver;
static int maxFrames = 20000;
static uint32_t traceAddr[4], traceSize[4], traceLast[NUM_GBAS][4];
static const char* traceName[4];
static int numTraces;
static const char* shotDir = ".";
static volatile int stop;

static uint32_t symAddr(const char* name) {
	for (int i = 0; i < numSyms; ++i) {
		if (!strcmp(syms[i].name, name)) {
			return syms[i].addr;
		}
	}
	return 0;
}

// lockstep callbacks: the master (0) runs ahead posting cycles, the slave may only use posted cycles
static pthread_mutex_t lsMutex;

static void lsLock(struct mLockstep* l) {
	(void) l;
	pthread_mutex_lock(&lsMutex);
}

static void lsUnlock(struct mLockstep* l) {
	(void) l;
	pthread_mutex_unlock(&lsMutex);
}

static bool lsSignal(struct mLockstep* l, unsigned mask) {
	(void) l;
	struct Gba* p = &gba[0];
	p->waitMask &= ~mask;
	if (!p->waitMask && p->awake < 1) {
		mCoreThreadStopWaiting(&p->thread);
		p->awake = 1;
		return true;
	}
	return false;
}

static bool lsWait(struct mLockstep* l, unsigned mask) {
	(void) l;
	struct Gba* p = &gba[0];
	p->waitMask |= mask;
	if (p->awake > 0) {
		mCoreThreadWaitFromThread(&p->thread);
		p->awake = 0;
		return true;
	}
	return false;
}

static void lsAddCycles(struct mLockstep* l, int id, int32_t cycles) {
	(void) l;
	if (!id) {
		for (int i = 1; i < NUM_GBAS; ++i) {
			struct Gba* p = &gba[i];
			p->cyclesPosted += cycles;
			if (p->awake < 1) {
				p->node.nextEvent += p->cyclesPosted;
			}
			mCoreThreadStopWaiting(&p->thread);
			p->awake = 1;
		}
	} else {
		gba[id].cyclesPosted += cycles;
	}
}

static int32_t lsUseCycles(struct mLockstep* l, int id, int32_t cycles) {
	(void) l;
	struct Gba* p = &gba[id];
	p->cyclesPosted -= cycles;
	if (p->cyclesPosted <= 0) {
		mCoreThreadWaitFromThread(&p->thread);
		p->awake = 0;
	}
	return p->cyclesPosted;
}

static int32_t lsUnusedCycles(struct mLockstep* l, int id) {
	(void) l;
	return gba[id].cyclesPosted;
}

static void lsUnload(struct mLockstep* l, int id) {
	(void) l;
	if (id) {
		gba[id].cyclesPosted = 0;
		struct Gba* p = &gba[0];
		p->waitMask &= ~(1 << id);
		if (!p->waitMask && p->awake < 1) {
			mCoreThreadStopWaiting(&p->thread);
			p->awake = 1;
		}
	} else {
		for (int i = 1; i < NUM_GBAS; ++i) {
			struct Gba* p = &gba[i];
			p->cyclesPosted += lockstep.players[0]->eventDiff;
			if (p->awake < 1) {
				p->node.nextEvent += p->cyclesPosted;
				mCoreThreadStopWaiting(&p->thread);
				p->awake = 1;
			}
		}
	}
}

static void quietLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	(void) logger;
	(void) category;
	static int verbose = -1;
	if (verbose < 0) {
		verbose = getenv("LINKTEST_LOG") != NULL;
	}
	if (verbose || (level & (mLOG_FATAL | mLOG_ERROR))) {
		vfprintf(stderr, format, args);
		fputc('\n', stderr);
	}
}

static struct mLogger logger = { .log = quietLog };

static uint32_t hashRange(struct mCore* core, uint32_t addr, uint32_t size) {
	uint32_t h = 2166136261u;
	for (uint32_t i = 0; i < size; ++i) {
		h = (h ^ core->busRead8(core, addr + i)) * 16777619u;
	}
	return h;
}

static void writeBmp(struct Gba* g, int frame) {
	char path[512];
	snprintf(path, sizeof(path), "%s/gba%d_%06d.bmp", shotDir, g->id, frame);
	FILE* f = fopen(path, "wb");
	if (!f) {
		return;
	}
	const int w = GBA_VIDEO_HORIZONTAL_PIXELS, h = GBA_VIDEO_VERTICAL_PIXELS;
	uint32_t dataSize = w * h * 3;
	uint8_t header[54] = { 'B', 'M' };
	uint32_t fileSize = 54 + dataSize;
	memcpy(&header[2], &fileSize, 4);
	header[10] = 54;
	header[14] = 40;
	memcpy(&header[18], &w, 4);
	memcpy(&header[22], &h, 4);
	header[26] = 1;
	header[28] = 24;
	fwrite(header, 1, 54, f);
	for (int y = h - 1; y >= 0; --y) {
		for (int x = 0; x < w; ++x) {
			uint32_t c = g->video[y * w + x];
			uint8_t bgr[3] = { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
			fwrite(bgr, 1, 3, f);
		}
	}
	fclose(f);
}

static uint32_t botRand(struct Gba* g) {
	g->rng ^= g->rng << 13;
	g->rng ^= g->rng >> 17;
	g->rng ^= g->rng << 5;
	return g->rng;
}

static void frameCallback(struct mCoreThread* thread) {
	struct Gba* g = thread->userData;
	struct mCore* core = g->core;
	int frame = g->frames++;

	if (g->resetAt && frame == g->resetAt) {
		core->reset(core);
		g->scriptLen = 0;
	}

	for (int i = 0; i < g->numShots; ++i) {
		if (g->shots[i] == frame) {
			writeBmp(g, frame);
		}
	}

	for (int i = 0; i < numTraces; ++i) {
		uint32_t v = traceSize[i] == 1 ? core->busRead8(core, traceAddr[i]) : core->busRead16(core, traceAddr[i]);
		if (v != traceLast[g->id][i] || frame == 0) {
			printf("gba%d frame %d: %s = %x\n", g->id, frame, traceName[i], v);
			traceLast[g->id][i] = v;
		}
	}

	int inGame = (core->busRead8(core, symNumPlayers) == 2);
	int timer = core->busRead16(core, symTimer);
	int frameDone = core->busRead16(core, symFrameDone);
	int matchOver = core->busRead8(core, symMatchOver);

	// record the game state once per game frame, when that frame's logic is complete
	if (inGame && !matchOver && timer && frameDone == timer && timer != g->lastFrameDone) {
		g->lastFrameDone = timer;
		uint32_t* h = &g->hashes[timer * (numSyms + 1)];
		uint32_t all = 0;
		for (int i = 0; i < numSyms; ++i) {
			h[i + 1] = hashRange(core, syms[i].addr, syms[i].size);
			all = all * 31 + h[i + 1];
		}
		h[0] = all;
		g->seen[timer] = 1;
		if (!g->gameFramesDone++) {
			g->firstGameFrame = frame;
		} else {
			int gap = frame - g->lastGameFrame;
			g->gaps[gap > 3 ? 3 : gap - 1]++;
		}
		g->lastGameFrame = frame;
	}
	if (inGame && !matchOver) {
		g->gameFrames++;
	}
	if (matchOver && !g->ended) {
		g->ended = frame;
		printf("gba%d: match over at frame %d\n", g->id, frame);
		if (g->numShots < MAX_SHOTS) {
			g->shots[g->numShots++] = frame + 70;
		}
	}

	int keys = 0;
	for (int i = 0; i < g->scriptLen; ++i) {
		if (frame >= g->script[i].frame && frame < g->script[i].frame + g->script[i].len) {
			keys |= g->script[i].keys;
		}
	}
	if (inGame && !matchOver) {
		// random play: hold a direction for a while, sometimes drop a bomb
		if (g->botHold-- <= 0) {
			static const int dirs[] = { 0, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT };
			g->botKeys = dirs[botRand(g) % 5];
			if (botRand(g) % 8 == 0) {
				g->botKeys |= KEY_A;
			}
			g->botHold = 8 + botRand(g) % 24;
		}
		keys |= g->botKeys;
	}
	if (g->pauseAt && inGame && !matchOver && timer >= g->pauseAt && g->pauseStep < 130) {
		// press start, wait two seconds, press start again
		int step = g->pauseStep++;
		if (step < 3 || (step >= 120 && step < 123)) {
			keys = KEY_START;
		} else {
			keys = 0;
		}
	}
	g->lastTimer = timer;
	core->setKeys(core, keys);

	if (frame >= maxFrames || (g->ended && frame > g->ended + 90)) {
		stop = 1;
	}
}

static void usage(void) {
	fprintf(stderr, "usage: linktest [options] rom.gba\n"
	        "  --bios FILE            use a real BIOS (needed for multiboot)\n"
	        "  --rom2 FILE|none       ROM for the second GBA (default same, none = boot BIOS with no cart)\n"
	        "  --sym NAME=ADDR:SIZE   game state to compare (also needs universalTimer, frameDone,\n"
	        "                         numPlayers, matchOver)\n"
	        "  --keys GBA:FRAME:KEYS:LEN   scripted input (KEYS as a GBA key mask)\n"
	        "  --seed GBA:N           seed for random play\n"
	        "  --pause GBA:TIMER      press start at that game frame and again 2s later\n"
	        "  --reset GBA:FRAME      reset a Gameboy, as if switched off mid-game\n"
	        "  --trace SYM            print a (1 or 2 byte) symbol whenever it changes\n"
	        "  --shot GBA:FRAME       save a screenshot\n"
	        "  --shots DIR            where to save screenshots\n"
	        "  --frames N             give up after N frames\n");
	exit(2);
}

int main(int argc, char** argv) {
	const char* rom = NULL;
	const char* rom2 = NULL;
	const char* bios = NULL;

	for (int i = 0; i < NUM_GBAS; ++i) {
		gba[i].id = i;
		gba[i].rng = 0x12345678 + i * 0x9E3779B9;
		gba[i].lastTimer = -1;
		gba[i].lastFrameDone = -1;
	}

	for (int i = 1; i < argc; ++i) {
		const char* a = argv[i];
		const char* v = (i + 1 < argc) ? argv[i + 1] : NULL;
		int n, f, k, l;
		if (!strcmp(a, "--bios") && v) {
			bios = v; ++i;
		} else if (!strcmp(a, "--rom2") && v) {
			rom2 = v; ++i;
		} else if (!strcmp(a, "--sym") && v && numSyms < MAX_SYMS) {
			struct Sym* s = &syms[numSyms++];
			if (sscanf(v, "%47[^=]=%x:%x", s->name, &s->addr, &s->size) != 3) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--keys") && v && sscanf(v, "%d:%d:%d:%d", &n, &f, &k, &l) == 4) {
			gba[n].script[gba[n].scriptLen++] = (struct ScriptStep) { f, k, l };
			++i;
		} else if (!strcmp(a, "--seed") && v && sscanf(v, "%d:%d", &n, &k) == 2) {
			gba[n].rng = (uint32_t) k * 2654435761u + 1;
			++i;
		} else if (!strcmp(a, "--pause") && v && sscanf(v, "%d:%d", &n, &k) == 2) {
			gba[n].pauseAt = k;
			++i;
		} else if (!strcmp(a, "--trace") && v && numTraces < 4) {
			traceName[numTraces++] = v;
			++i;
		} else if (!strcmp(a, "--reset") && v && sscanf(v, "%d:%d", &n, &f) == 2) {
			gba[n].resetAt = f;
			++i;
		} else if (!strcmp(a, "--shot") && v && sscanf(v, "%d:%d", &n, &f) == 2) {
			gba[n].shots[gba[n].numShots++] = f;
			++i;
		} else if (!strcmp(a, "--shots") && v) {
			shotDir = v; ++i;
		} else if (!strcmp(a, "--frames") && v) {
			maxFrames = atoi(v); ++i;
		} else if (a[0] != '-' && !rom) {
			rom = a;
		} else {
			fprintf(stderr, "bad option: %s\n", a);
			usage();
		}
	}
	if (!rom) {
		usage();
	}
	for (int i = 0; i < numTraces; ++i) {
		traceAddr[i] = symAddr(traceName[i]);
		for (int j = 0; j < numSyms; ++j) {
			if (!strcmp(syms[j].name, traceName[i])) {
				traceSize[i] = syms[j].size;
			}
		}
	}
	symFrameDone = symAddr("frameDone");
	symTimer = symAddr("universalTimer");
	symNumPlayers = symAddr("numPlayers");
	symMatchOver = symAddr("matchOver");
	if (!symFrameDone || !symTimer || !symNumPlayers || !symMatchOver) {
		fprintf(stderr, "need --sym for frameDone, universalTimer, numPlayers and matchOver\n");
		return 2;
	}

	mLogSetDefaultLogger(&logger);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lsMutex, &attr);

	GBASIOLockstepInit(&lockstep);
	ls = &lockstep.d;
	ls->lock = lsLock;
	ls->unlock = lsUnlock;
	ls->signal = lsSignal;
	ls->wait = lsWait;
	ls->addCycles = lsAddCycles;
	ls->useCycles = lsUseCycles;
	ls->unusedCycles = lsUnusedCycles;
	ls->unload = lsUnload;

	for (int i = 0; i < NUM_GBAS; ++i) {
		struct Gba* g = &gba[i];
		g->core = GBACoreCreate();
		g->core->init(g->core);
		mCoreInitConfig(g->core, NULL);
		mCoreConfigSetValue(&g->core->config, "useBios", bios ? "1" : "0");
		const char* path = (i && rom2) ? rom2 : rom;
		int noCart = !strcmp(path, "none");
		// a Gameboy with no cartridge has to run its BIOS to wait for multiboot
		mCoreConfigSetValue(&g->core->config, "skipBios", noCart ? "0" : "1");
		mCoreLoadConfig(g->core);

		g->video = calloc(GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS, sizeof(color_t));
		g->core->setVideoBuffer(g->core, g->video, GBA_VIDEO_HORIZONTAL_PIXELS);

		if (bios) {
			struct VFile* vf = VFileOpen(bios, O_RDONLY);
			if (!vf || !g->core->loadBIOS(g->core, vf, 0)) {
				fprintf(stderr, "can't load BIOS %s\n", bios);
				return 2;
			}
		}
		if (!noCart) {
			struct VFile* vf = VFileOpen(path, O_RDONLY);
			if (!vf || !g->core->loadROM(g->core, vf)) {
				fprintf(stderr, "can't load ROM %s\n", path);
				return 2;
			}
		}

		GBASIOLockstepNodeCreate(&g->node);
		GBASIOLockstepAttachNode(&lockstep, &g->node);
		struct GBA* board = g->core->board;
		GBASIOSetDriver(&board->sio, &g->node.d, SIO_MULTI);

		g->hashes = calloc(65536 * (numSyms + 1), sizeof(uint32_t));
		g->seen = calloc(65536, 1);
		g->awake = 1;

		memset(&g->thread, 0, sizeof(g->thread));
		g->thread.core = g->core;
		g->thread.userData = g;
		g->thread.frameCallback = frameCallback;
		g->thread.logger.logger = &logger;
	}

	for (int i = 0; i < NUM_GBAS; ++i) {
		mCoreThreadStart(&gba[i].thread);
	}
	while (!stop) {
		usleep(10000);
		for (int i = 0; i < NUM_GBAS; ++i) {
			if (mCoreThreadHasCrashed(&gba[i].thread)) {
				fprintf(stderr, "gba%d crashed\n", i);
				stop = 1;
			}
		}
	}
	for (int i = 0; i < NUM_GBAS; ++i) {
		mCoreThreadEnd(&gba[i].thread);
	}
	for (int i = 0; i < NUM_GBAS; ++i) {
		mCoreThreadStopWaiting(&gba[i].thread);
	}
	for (int i = 0; i < NUM_GBAS; ++i) {
		mCoreThreadJoin(&gba[i].thread);
	}

	for (int i = 0; i < numSyms; ++i) {
		if (syms[i].size <= 4) {
			printf("%s:", syms[i].name);
			for (int j = 0; j < NUM_GBAS; ++j) {
				uint32_t v = 0;
				for (uint32_t b = 0; b < syms[i].size; ++b) {
					v |= gba[j].core->busRead8(gba[j].core, syms[i].addr + b) << (8 * b);
				}
				printf(" %x", v);
			}
			printf("\n");
		}
	}

	// compare the two games frame by frame
	int compared = 0, onlyOne = 0, firstBad = -1;
	for (int t = 0; t < 65536; ++t) {
		if (gba[0].seen[t] && gba[1].seen[t]) {
			++compared;
			if (gba[0].hashes[t * (numSyms + 1)] != gba[1].hashes[t * (numSyms + 1)] && firstBad < 0) {
				firstBad = t;
			}
		} else if (gba[0].seen[t] || gba[1].seen[t]) {
			++onlyOne;
		}
	}
	for (int i = 0; i < NUM_GBAS; ++i) {
		printf("gba%d: %d frames, %d game frames in %d video frames\n", i, gba[i].frames, gba[i].gameFramesDone,
		       gba[i].lastGameFrame - gba[i].firstGameFrame + 1);
		printf("gba%d: game frames took 1 video frame %d times, 2: %d, 3: %d, more (banners): %d\n", i,
		       gba[i].gaps[0], gba[i].gaps[1], gba[i].gaps[2], gba[i].gaps[3]);
	}
	printf("compared %d game frames (%d seen by only one)\n", compared, onlyOne);
	if (firstBad >= 0) {
		printf("DESYNC at game frame %d in:", firstBad);
		for (int i = 0; i < numSyms; ++i) {
			if (gba[0].hashes[firstBad * (numSyms + 1) + i + 1] != gba[1].hashes[firstBad * (numSyms + 1) + i + 1]) {
				printf(" %s", syms[i].name);
			}
		}
		printf("\n");
		return 1;
	}
	if (!compared) {
		printf("NOTHING COMPARED, the game never started\n");
		return 1;
	}
	printf("IN SYNC\n");
	return 0;
}
