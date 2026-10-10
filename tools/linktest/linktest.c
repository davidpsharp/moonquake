// linktest: run two copies of a GBA ROM linked by an emulated link cable (libmgba), drive
// them with scripted and random input, and check the two games stay in step.
//
// Every frame the game logic finishes (the game writes universalTimer to frameDone) each
// emulator hashes the game state symbols given with --sym. At the end the hashes for each
// universalTimer value are compared between the two Gameboys; any difference is a desync.
//
// See tools/linktest/run.sh for usage.

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/lockstep.h>
#include <mgba/core/log.h>
#include <mgba/core/thread.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio/lockstep.h>
#include <mgba-util/vfs.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_GBAS_TESTED 4
#define MAX_SYMS 32
#define MAX_SCRIPT 64
#define MAX_SHOTS 512

#define KEY_A       (1 << 0)
#define KEY_B       (1 << 1)
#define KEY_SELECT  (1 << 2)
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
	int quitAt;             // game frame to leave the game from the pause menu at, 0 for never
	int lobbyPress;
	int lobbyRights;
	int lobbyWait;
	int afterDone;
	int leave;              // tap select during the game, so leave once out
	int unplugWhenOut;      // pull the cable out once out of the game
	int unplugAt;           // pull the cable out at this frame, 0 for never
	volatile int unplugRequested;
	int unplugged;
	int noCart;             // waiting to be sent the game by multiboot, until gameLoaded
	int gameLoaded;
	int gameRunningFrames;  // frames since a Gameboy sent the game was seen running it
	int botAlways;          // random play outside linked games too (after the menus)
	int idle;               // no random play: the player stands still
	int ghosts;             // report dead robots whose sprites are on screen
	int checkBoard;         // report rubble or flames appearing with no flame to cause them
	uint8_t board[19 * 13]; // the board at the last game frame
	int haveBoard;
	int lastStatus[4];
	FILE* audio;            // sound output, 16 bit stereo
	long audioSamples;
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
	uint8_t* players;       // the player array per universalTimer, to show what differs
	uint8_t* seen;
	int ended;              // matchOver seen
	int shots[MAX_SHOTS];
	int numShots;
};

static struct GBASIOLockstep lockstep;
static struct mLockstep* ls;
static struct Gba gba[MAX_GBAS_TESTED];
static int numGbas = 2;
static struct Sym syms[MAX_SYMS];
static int numSyms;
static uint32_t symFrameDone, symTimer, symLinked, symMatchOver;
static struct Sym infos[MAX_SYMS];      // read but not compared
static int numInfos;

static uint32_t symAddr(const char* name) {
	for (int i = 0; i < numSyms; ++i) {
		if (!strcmp(syms[i].name, name)) {
			return syms[i].addr;
		}
	}
	for (int i = 0; i < numInfos; ++i) {
		if (!strcmp(infos[i].name, name)) {
			return infos[i].addr;
		}
	}
	return 0;
}
static int lobbyPlayers;                // GBA 0 presses start in the lobby when this many are waiting
static int lobbyLevel;                  // and first presses right this many times to choose the level
static const char* carts[MAX_GBAS_TESTED];
static int maxFrames = 20000;
static int afterMatch;      // frames to carry on after a match (pressing A to clear the result)
static uint32_t traceAddr[4], traceSize[4], traceLast[MAX_GBAS_TESTED][4];
static const char* traceName[4];
static int numTraces;
static const char* shotDir;
static volatile int stop;
static int siFlicker;   // make the other Gameboys' SI bit read low at random, as on real hardware
struct Poke {
	int gba, frame;
	uint32_t addr, value;
	int gameFrame;          // frame is a game frame (universalTimer), on every Gameboy
};
static struct Poke pokes[32];
static int numPokes;
struct Dump {
	int gba, frame;
	uint32_t addr, len;
	char file[256];
};
static struct Dump dumps[32];
static int numDumps;
static uint32_t symPlayer, symRobot;
static uint32_t symArea, symNuked;

// --check-board: each game frame, look for rubble starting to explode, or a flame appearing,
// with no flame next to it to have caused it, and for the screen's map not matching the board
#define AX 19
#define AY 13
#define IS_FLAME(t) ((t) >= 84 && (t) <= 220)
#define IS_RUBBLE_EXPLO(t) ((t) >= 52 && (t) <= 80)
struct Gba;
static void checkBoard(struct Gba* g, struct mCore* core, int timer) {
	uint8_t now[AX * AY];
	for (int i = 0; i < AX * AY; ++i) {
		now[i] = core->busRead8(core, symArea + i);
	}
	int nuked = symNuked ? core->busRead8(core, symNuked) : 0;
	if (g->haveBoard && !nuked) {
		for (int x = 0; x < AX; ++x) {
			for (int y = 0; y < AY; ++y) {
				uint8_t was = g->board[x * AY + y], is = now[x * AY + y];
				int newRubble = was == 4 && IS_RUBBLE_EXPLO(is);
				int newFlame = !IS_FLAME(was) && IS_FLAME(is) && is != 84 && !(was >= 20 && was <= 48);
				if (!newRubble && !newFlame) {
					continue;
				}
				// something next to it must be a flame (or the bomb's centre) now
				int ok = 0;
				static const int d[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
				for (int k = 0; k < 4; ++k) {
					int nx = x + d[k][0], ny = y + d[k][1];
					if (nx >= 0 && nx < AX && ny >= 0 && ny < AY && IS_FLAME(now[nx * AY + ny])) {
						ok = 1;
					}
				}
				if (!ok) {
					printf("gba%d timer %d: %s at %d,%d (%d -> %d) with no flame next to it\n", g->id, timer,
					       newRubble ? "rubble exploded" : "flame appeared", x, y, was, is);
				}
			}
		}
	}
	// the map on screen (64x32 tiles in two 32x32 blocks) should show the board
	for (int x = 0; x < AX; ++x) {
		for (int y = 0; y < AY; ++y) {
			int tx = x * 2, ty = y * 2;
			uint32_t addr = 0x06000000 + 2 * (tx < 32 ? ty * 32 + tx : (tx - 32) + ty * 32 + 1024);
			int tile = core->busRead16(core, addr);
			// (a tile changed by --poke-game is drawn when the game next changes it)
			int poked = 0;
			for (int i = 0; i < numPokes; ++i) {
				poked |= pokes[i].gameFrame && pokes[i].addr == symArea + x * AY + y;
			}
			if (!poked && tile != now[x * AY + y] && !(g->haveBoard && tile == g->board[x * AY + y])) {
				printf("gba%d timer %d: screen shows tile %d at %d,%d, the board has %d\n", g->id, timer, tile, x, y, now[x * AY + y]);
			}
		}
	}
	memcpy(g->board, now, sizeof now);
	g->haveBoard = 1;
}
#define PLAYER_SIZE 48          // sizeof(struct Player) in the game
#define PLAYER_LIFE_STATUS 18   // offsetof(struct Player, lifeStatus)
#define LIFE_OUT 3



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
		for (int i = 1; i < numGbas; ++i) {
			struct Gba* p = &gba[i];
			if (p->unplugged) {
				continue;
			}
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
		if (!lockstep.players[0]) {
			return;
		}
		for (int i = 1; i < numGbas; ++i) {
			struct Gba* p = &gba[i];
			if (p->unplugged) {
				continue;
			}
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
	if (!shotDir) {
		return;
	}
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

	// (drained from the start: once mGBA's buffer's full it stops producing sound)
	if (g->audio) {
		short buf[2048 * 2];
		struct blip_t* left = core->getAudioChannel(core, 0);
		struct blip_t* right = core->getAudioChannel(core, 1);
		int n = blip_samples_avail(left);
		if (n > 2048) {
			n = 2048;
		}
		blip_read_samples(left, buf, n, 1);
		blip_read_samples(right, buf + 1, n, 1);
		fwrite(buf, 4, n, g->audio);
		g->audioSamples += n;
	}
	if (getenv("LINKTEST_AUDIO_DEBUG") && (frame == 100 || frame == 6000)) {
		struct GBA* b = core->board;
		fprintf(stderr, "gba%d frame %d: masterVolume %d, mute %d\n", g->id, frame, b->audio.masterVolume, core->opts.mute);
	}

	// On real hardware a Gameboy that isn't player 1 reads its SI pin low during transfers
	// (player 1 starts one about every millisecond), mGBA keeps it high: flip it at random.
	// (Not before a Gameboy with no cartridge has the game: its BIOS reads SI too, and this
	// flips it when idle, which real hardware doesn't.)
	if (siFlicker && g->id > 0 && !(g->noCart && !g->gameLoaded)) {
		struct GBA* board = core->board;
		uint16_t si = (botRand(g) & 1) ? 4 : 0;
		board->sio.siocnt = (board->sio.siocnt & ~4) | si;
		board->memory.io[REG_SIOCNT >> 1] = (board->memory.io[REG_SIOCNT >> 1] & ~4) | si;
	}

	for (int i = 0; i < numPokes; ++i) {
		if (!pokes[i].gameFrame && pokes[i].gba == g->id && pokes[i].frame == frame) {
			core->busWrite8(core, pokes[i].addr, pokes[i].value);
		}
	}
	for (int d = 0; d < numDumps; ++d) {
		if (dumps[d].gba == g->id && dumps[d].frame == frame) {
			FILE* f = fopen(dumps[d].file, "wb");
			for (uint32_t i = 0; f && i < dumps[d].len; ++i) {
				fputc(core->busRead8(core, dumps[d].addr + i), f);
			}
			if (f) {
				fclose(f);
			}
		}
	}

	if (g->resetAt && frame == g->resetAt) {
		core->reset(core);
		g->scriptLen = 0;
	}

	for (int i = 0; i < g->numShots; ++i) {
		if (g->shots[i] == frame) {
			writeBmp(g, frame);
		}
	}

	for (int i = 0; i < numTraces && !(g->noCart && !g->gameLoaded); ++i) {
		uint32_t v = traceSize[i] == 1 ? core->busRead8(core, traceAddr[i]) : core->busRead16(core, traceAddr[i]);
		if (v != traceLast[g->id][i] || frame == 0) {
			printf("gba%d frame %d: %s = %x\n", g->id, frame, traceName[i], v);
			traceLast[g->id][i] = v;
		}
	}

	// A Gameboy with no cartridge has no game until it's been sent one: until then whatever's
	// at the game's addresses is from the BIOS. The header arrives first (in EWRAM, where the
	// game runs, and where mGBA loads a ROM linked for it), so wait until it matches player 1's
	// and the CPU's been seen running code there, then a few frames for the game's start up.
	if (g->noCart && !g->gameLoaded) {
		struct mCore* cart = gba[0].core;
		int same = core->busRead32(core, 0x020000A0) != 0;
		for (uint32_t a = 0xA0; a < 0xC0; a += 4) {
			if (core->busRead32(core, 0x02000000 + a) != cart->busRead32(cart, 0x02000000 + a)) {
				same = 0;
			}
		}
		uint32_t pc = ((struct GBA*) core->board)->cpu->gprs[15];
		if (same && (g->gameRunningFrames || (pc >= 0x02000000 && pc < 0x03000000))) {
			g->gameRunningFrames++;
		}
		if (g->gameRunningFrames > 10) {
			g->gameLoaded = 1;
			printf("gba%d: game arrived by multiboot, running at frame %d\n", g->id, frame);
		} else {
			core->setKeys(core, 0);
			return;
		}
	}

	int inGame = core->busRead8(core, symLinked);

	// player 1 starts the game once enough players are waiting in the lobby
	if (g->id == 0 && lobbyPlayers && !inGame && !g->ended) {
		uint32_t words = 0;
		for (int i = 0; i < numInfos; ++i) {
			if (!strcmp(infos[i].name, "receivedWord")) {
				words = infos[i].addr;
			}
		}
		int waiting = 1;
		for (int i = 1; i < 4; ++i) {
			if (core->busRead16(core, words + i * 2) == 0x8001) {
				++waiting;
			}
		}
		if (waiting >= lobbyPlayers && lobbyLevel && g->lobbyWait++ < 120) {
			// give the screen time to show everyone before choosing the level (it doesn't
			// read buttons while it's writing)
			core->setKeys(core, 0);
			return;
		}
		if ((waiting >= lobbyPlayers || g->lobbyRights) && g->lobbyRights < lobbyLevel * 8) {
			// choose the level: tap right (pressed 4 frames, let go 4), all the taps once
			// started (the count of who's waiting can drop for a moment while player 1 looks
			// for Gameboys to send the game to, and letting go mid tap would make it two)
			core->setKeys(core, (g->lobbyRights++ % 8) < 4 ? KEY_RIGHT : 0);
			return;
		}
		if (waiting >= lobbyPlayers) {
			// tap start (the lobby wants it pressed after being let go)
			if (!g->lobbyPress++) {
				printf("gba0 frame %d: %d players waiting, pressing start\n", frame, waiting);
			}
			core->setKeys(core, (frame / 8) % 2 ? KEY_START : 0);
			return;
		}
	}
	int timer = core->busRead16(core, symTimer);
	int frameDone = core->busRead16(core, symFrameDone);
	int matchOver = core->busRead8(core, symMatchOver);

	// record the game state once per game frame, when that frame's logic is complete
	// (an unplugged Gameboy finishes the frame it was on its own, then the link's lost)
	if (inGame && !matchOver && !g->unplugged && timer && frameDone == timer && timer != g->lastFrameDone) {
		g->lastFrameDone = timer;
		uint32_t* h = &g->hashes[timer * (numSyms + 1)];
		uint32_t all = 0;
		for (int i = 0; i < numSyms; ++i) {
			h[i + 1] = hashRange(core, syms[i].addr, syms[i].size);
			all = all * 31 + h[i + 1];
		}
		h[0] = all;
		g->seen[timer] = 1;
		for (int i = 0; i < 4 * PLAYER_SIZE; ++i) {
			g->players[timer * 4 * PLAYER_SIZE + i] = core->busRead8(core, symPlayer + i);
		}
		// game state changes for every Gameboy, at the end of the same game frame
		for (int i = 0; i < numPokes; ++i) {
			if (pokes[i].gameFrame && pokes[i].frame == timer) {
				core->busWrite8(core, pokes[i].addr, pokes[i].value);
			}
		}
		if (g->checkBoard) {
			checkBoard(g, core, timer);
			// a player who's out stays out
			for (int i = 0; i < 4; ++i) {
				int status = core->busRead8(core, symPlayer + i * PLAYER_SIZE + PLAYER_LIFE_STATUS);
				if (g->lastStatus[i] == LIFE_OUT && status != LIFE_OUT) {
					printf("gba%d timer %d: player %d was out, now life status %d\n", g->id, timer, i, status);
				}
				g->lastStatus[i] = status;
			}
		}
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
	if (g->ghosts && symRobot) {
		// a dead robot's sprite shouldn't be on screen (OAM entries 100 on are the robots)
		for (int i = 0; i < 10; ++i) {
			int dead = core->busRead8(core, symRobot + i * 10);
			uint16_t a0 = core->busRead16(core, 0x07000000 + (100 + i) * 8);
			uint16_t a1 = core->busRead16(core, 0x07000000 + (100 + i) * 8 + 2);
			uint16_t a2 = core->busRead16(core, 0x07000000 + (100 + i) * 8 + 4);
			int x = a1 & 511, y = a0 & 255;
			int visible = (x < 240 || x > 512 - 16) && (y < 160 || y > 256 - 16);
			if (dead == 2 && visible) {
				printf("gba%d frame %d: dead robot %d's sprite on screen at %d,%d, char %d\n", g->id, frame, i, x, y, a2 & 1023);
			}
		}
	}

	if ((inGame || (g->botAlways && frame > 700)) && !matchOver && !g->idle) {
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
		if (g->leave && (frame / 8) % 4 == 0) {
			keys = KEY_SELECT;
		}
	}
	if (g->unplugAt && frame >= g->unplugAt && !g->unplugged) {
		g->unplugRequested = 1;
	}
	if (g->unplugWhenOut && !g->unplugged && inGame &&
	    core->busRead8(core, symPlayer + g->id * PLAYER_SIZE + PLAYER_LIFE_STATUS) == LIFE_OUT) {
		g->unplugRequested = 1;
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
	if (g->quitAt && inGame && !matchOver && timer >= g->quitAt && g->pauseStep < 130) {
		// pause, down to LEAVE GAME, A, down to YES, A
		static const struct { int step, keys; } script[] = {
			{ 0, KEY_START }, { 40, KEY_DOWN }, { 60, KEY_A }, { 90, KEY_DOWN }, { 110, KEY_A }
		};
		int step = g->pauseStep++;
		keys = 0;
		for (unsigned j = 0; j < sizeof script / sizeof script[0]; j++) {
			if (step >= script[j].step && step < script[j].step + 3) {
				keys = script[j].keys;
			}
		}
	}
	g->lastTimer = timer;
	core->setKeys(core, keys);

	if (afterMatch && g->ended) {
		// clear the result screen, then leave the menu be
		keys = (frame > g->ended + 120 && frame < g->ended + 125) ? KEY_A : 0;
		core->setKeys(core, keys);
		if (g->numShots < MAX_SHOTS && frame == g->ended + afterMatch - 5) {
			g->shots[g->numShots++] = frame + 1;
		}
		if (frame == g->ended + afterMatch - 4 && shotDir) {
			// and the sprites, to see which menu entry's highlighted
			char path[512];
			snprintf(path, sizeof(path), "%s/oam%d.bin", shotDir, g->id);
			FILE* f = fopen(path, "wb");
			for (int i = 0; f && i < 1024; ++i) {
				fputc(core->busRead8(core, 0x07000000 + i), f);
			}
			if (f) {
				fclose(f);
			}
		}
	}
	if (afterMatch) {
		// with --after-match, stop once every Gameboy's had its time after the match
		if (g->ended && frame > g->ended + afterMatch) {
			g->afterDone = 1;
		}
		int allDone = 1;
		for (int i = 0; i < numGbas; ++i) {
			allDone &= gba[i].afterDone;
		}
		if (allDone || frame >= maxFrames) {
			stop = 1;
		}
	} else if (frame >= maxFrames || (g->ended && frame > g->ended + 90)) {
		stop = 1;
	}
}

static void usage(void) {
	fprintf(stderr, "usage: linktest [options] rom.gba\n"
	        "  --bios FILE            use a real BIOS (needed for multiboot)\n"
	        "  --gbas N               number of Gameboys, 2-4 (default 2)\n"
	        "  --cart GBA:FILE|none   ROM for one Gameboy (default the same, none = no cartridge,\n"
	        "                         boot the BIOS to wait for multiboot)\n"
	        "  --si-flicker           make the other Gameboys' SI bit read low at random, as on real\n"
	        "                         hardware during transfers\n"
	        "  --after-match N        run N frames after a match (pressing A to clear the result),\n"
	        "                         and save a screenshot at the end\n"
	        "  --lobby N              GBA 0 presses start in the lobby once N players are waiting\n"
	        "  --level N              and first chooses level N (presses right N times)\n"
	        "                         (needs --info receivedWord=...)\n"
	        "  --info NAME=ADDR:SIZE  a symbol to read but not compare\n"
	        "  --leave GBA            tap select in the game, so leave once out of it\n"
	        "  --unplug-out GBA       pull a Gameboy's cable out once its player's out of the game\n"
	        "  --dump GBA:FRAME:ADDR:LEN:FILE   save memory (hex address and length) to a file\n"
	        "  --poke-game TIMER:ADDR:VALUE   write a byte on every Gameboy at the end of that game frame\n"
	        "  --poke GBA:FRAME:ADDR:VALUE   write a byte (hex address and value) at that frame\n"
	        "  --idle GBA             no random play, the player stands still\n"
	        "  --bot GBA              random play in single player games too\n"
	        "  --check-board GBA      report rubble or flames appearing from nowhere (needs area)\n"
	        "  --ghosts GBA           report dead robots whose sprites are on screen (needs robot)\n"
	        "  --audio GBA:FILE       save the sound as raw 16 bit stereo\n"
	        "  --unplug GBA:FRAME     pull a Gameboy's cable out at that frame (use the last\n"
	        "                         Gameboys, mGBA renumbers the rest if one's taken from the middle)\n"
	        "  --sym NAME=ADDR:SIZE   game state to compare (also needs universalTimer, frameDone,\n"
	        "                         linked, matchOver)\n"
	        "  --keys GBA:FRAME:KEYS:LEN   scripted input (KEYS as a GBA key mask)\n"
	        "  --seed GBA:N           seed for random play\n"
	        "  --pause GBA:TIMER      press start at that game frame and again 2s later\n"
	        "  --quit GBA:TIMER       at that game frame, leave the game from the pause menu\n"
	        "  --reset GBA:FRAME      reset a Gameboy, as if switched off mid-game\n"
	        "  --trace SYM            print a (1 or 2 byte) symbol whenever it changes\n"
	        "  --shot GBA:FRAME       save a screenshot\n"
	        "  --shots DIR            where to save screenshots\n"
	        "  --frames N             give up after N frames\n");
	exit(2);
}

int main(int argc, char** argv) {
	const char* rom = NULL;
	const char* bios = NULL;

	for (int i = 0; i < MAX_GBAS_TESTED; ++i) {
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
		} else if (!strcmp(a, "--gbas") && v) {
			numGbas = atoi(v); ++i;
			if (numGbas < 2 || numGbas > MAX_GBAS_TESTED) {
				usage();
			}
		} else if (!strcmp(a, "--cart") && v && sscanf(v, "%d:", &n) == 1 && n < MAX_GBAS_TESTED) {
			carts[n] = strchr(v, ':') + 1;
			++i;
		} else if (!strcmp(a, "--info") && v && numInfos < MAX_SYMS) {
			struct Sym* s = &infos[numInfos++];
			if (sscanf(v, "%47[^=]=%x:%x", s->name, &s->addr, &s->size) != 3) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--leave") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].leave = 1;
			++i;
		} else if (!strcmp(a, "--unplug-out") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].unplugWhenOut = 1;
			++i;
		} else if (!strcmp(a, "--unplug") && v && sscanf(v, "%d:%d", &n, &f) == 2 && n < MAX_GBAS_TESTED) {
			gba[n].unplugAt = f;
			++i;
		} else if (!strcmp(a, "--dump") && v && numDumps < 32) {
			struct Dump* d = &dumps[numDumps++];
			if (sscanf(v, "%d:%d:%x:%x:%255s", &d->gba, &d->frame, &d->addr, &d->len, d->file) != 5) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--poke-game") && v && numPokes < 32) {
			struct Poke* pk = &pokes[numPokes++];
			pk->gameFrame = 1;
			if (sscanf(v, "%d:%x:%x", &pk->frame, &pk->addr, &pk->value) != 3) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--poke") && v && numPokes < 32) {
			struct Poke* pk = &pokes[numPokes++];
			if (sscanf(v, "%d:%d:%x:%x", &pk->gba, &pk->frame, &pk->addr, &pk->value) != 4) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--idle") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].idle = 1;
			++i;
		} else if (!strcmp(a, "--bot") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].botAlways = 1;
			++i;
		} else if (!strcmp(a, "--check-board") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].checkBoard = 1;
			++i;
		} else if (!strcmp(a, "--ghosts") && v && (n = atoi(v)) < MAX_GBAS_TESTED) {
			gba[n].ghosts = 1;
			++i;
		} else if (!strcmp(a, "--audio") && v) {
			static char file[256];
			if (sscanf(v, "%d:%255s", &n, file) != 2 || n >= MAX_GBAS_TESTED) {
				usage();
			}
			gba[n].audio = fopen(file, "wb");
			++i;
		} else if (!strcmp(a, "--si-flicker")) {
			siFlicker = 1;
		} else if (!strcmp(a, "--level") && v) {
			lobbyLevel = atoi(v); ++i;
		} else if (!strcmp(a, "--after-match") && v) {
			afterMatch = atoi(v); ++i;
		} else if (!strcmp(a, "--lobby") && v) {
			lobbyPlayers = atoi(v); ++i;
		} else if (!strcmp(a, "--sym") && v && numSyms < MAX_SYMS) {
			struct Sym* s = &syms[numSyms++];
			if (sscanf(v, "%47[^=]=%x:%x", s->name, &s->addr, &s->size) != 3) {
				usage();
			}
			++i;
		} else if (!strcmp(a, "--keys") && v && sscanf(v, "%d:%d:%d:%d", &n, &f, &k, &l) == 4 && n < MAX_GBAS_TESTED &&
		           gba[n].scriptLen < MAX_SCRIPT) {
			gba[n].script[gba[n].scriptLen++] = (struct ScriptStep) { f, k, l };
			++i;
		} else if (!strcmp(a, "--seed") && v && sscanf(v, "%d:%d", &n, &k) == 2 && n < MAX_GBAS_TESTED) {
			gba[n].rng = (uint32_t) k * 2654435761u + 1;
			++i;
		} else if (!strcmp(a, "--quit") && v && sscanf(v, "%d:%d", &n, &k) == 2 && n < MAX_GBAS_TESTED) {
			gba[n].quitAt = k;
			++i;
		} else if (!strcmp(a, "--pause") && v && sscanf(v, "%d:%d", &n, &k) == 2 && n < MAX_GBAS_TESTED) {
			gba[n].pauseAt = k;
			++i;
		} else if (!strcmp(a, "--trace") && v && numTraces < 4) {
			traceName[numTraces++] = v;
			++i;
		} else if (!strcmp(a, "--reset") && v && sscanf(v, "%d:%d", &n, &f) == 2 && n < MAX_GBAS_TESTED) {
			gba[n].resetAt = f;
			++i;
		} else if (!strcmp(a, "--shot") && v && sscanf(v, "%d:%d", &n, &f) == 2 && n < MAX_GBAS_TESTED &&
		           gba[n].numShots < MAX_SHOTS - 1) {
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
		for (int j = 0; j < numInfos; ++j) {
			if (!strcmp(infos[j].name, traceName[i])) {
				traceSize[i] = infos[j].size;
			}
		}
	}
	if (!lobbyPlayers) {
		lobbyPlayers = numGbas;
	}
	symFrameDone = symAddr("frameDone");
	symTimer = symAddr("universalTimer");
	symLinked = symAddr("linked");
	symPlayer = symAddr("player");
	symArea = symAddr("area");
	symNuked = symAddr("nuked");
	symRobot = symAddr("robot");
	symMatchOver = symAddr("matchOver");
	if (!symFrameDone || !symTimer || !symLinked || !symMatchOver) {
		fprintf(stderr, "need --sym for frameDone, universalTimer, linked and matchOver\n");
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

	for (int i = 0; i < numGbas; ++i) {
		struct Gba* g = &gba[i];
		g->core = GBACoreCreate();
		g->core->init(g->core);
		mCoreInitConfig(g->core, NULL);
		mCoreConfigSetValue(&g->core->config, "useBios", bios ? "1" : "0");
		const char* path = carts[i] ? carts[i] : rom;
		int noCart = !strcmp(path, "none");
		g->noCart = noCart;
		// a Gameboy with no cartridge has to run its BIOS to wait for multiboot
		mCoreConfigSetValue(&g->core->config, "skipBios", noCart ? "0" : "1");
		mCoreLoadConfig(g->core);
		// full volume, as the frontends set by default (with no config it's 0)
		g->core->opts.volume = 0x100;
		((struct GBA*) g->core->board)->audio.masterVolume = 0x100;

		g->video = calloc(GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS, sizeof(color_t));
		g->core->setVideoBuffer(g->core, g->video, GBA_VIDEO_HORIZONTAL_PIXELS);
		g->core->setAudioBufferSize(g->core, 4096);

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
		g->players = calloc(65536, 4 * PLAYER_SIZE);
		g->awake = 1;

		memset(&g->thread, 0, sizeof(g->thread));
		g->thread.core = g->core;
		g->thread.userData = g;
		g->thread.frameCallback = frameCallback;
		g->thread.logger.logger = &logger;
	}

	for (int i = 0; i < numGbas; ++i) {
		mCoreThreadStart(&gba[i].thread);
	}
	while (!stop) {
		usleep(10000);
		int requested = 0;
		for (int i = 0; i < numGbas; ++i) {
			requested |= gba[i].unplugRequested && !gba[i].unplugged;
		}
		if (requested) {
			// as mGBA's Qt frontend does: stop every Gameboy, take them off the cable
			for (int j = 0; j < numGbas; ++j) {
				mCoreThreadInterrupt(&gba[j].thread);
			}
			for (int i = 0; i < numGbas; ++i) {
				if (gba[i].unplugRequested && !gba[i].unplugged) {
					GBASIOLockstepDetachNode(&lockstep, &gba[i].node);
					struct GBA* board = gba[i].core->board;
					GBASIOSetDriver(&board->sio, NULL, SIO_MULTI);
					gba[i].unplugged = 1;
					gba[i].awake = 1;
					printf("gba%d: unplugged at frame %d\n", i, gba[i].frames);
				}
			}
			for (int j = 0; j < numGbas; ++j) {
				mCoreThreadContinue(&gba[j].thread);
			}
			// an unplugged Gameboy may have been left waiting for the cable, set it going alone
			for (int i = 0; i < numGbas; ++i) {
				if (gba[i].unplugged) {
					mCoreThreadStopWaiting(&gba[i].thread);
				}
			}
		}
		for (int i = 0; i < numGbas; ++i) {
			if (mCoreThreadHasCrashed(&gba[i].thread)) {
				fprintf(stderr, "gba%d crashed\n", i);
				stop = 1;
			}
		}
	}
	for (int i = 0; i < numGbas; ++i) {
		mCoreThreadEnd(&gba[i].thread);
	}
	for (int i = 0; i < numGbas; ++i) {
		mCoreThreadStopWaiting(&gba[i].thread);
	}
	for (int i = 0; i < numGbas; ++i) {
		mCoreThreadJoin(&gba[i].thread);
	}

	for (int i = 0; i < numSyms; ++i) {
		if (syms[i].size <= 4) {
			printf("%s:", syms[i].name);
			for (int j = 0; j < numGbas; ++j) {
				uint32_t v = 0;
				for (uint32_t b = 0; b < syms[i].size; ++b) {
					v |= gba[j].core->busRead8(gba[j].core, syms[i].addr + b) << (8 * b);
				}
				printf(" %x", v);
			}
			printf("\n");
		}
	}

	// compare the games frame by frame, between the Gameboys that got to each frame (one might
	// have left)
	int compared = 0, notAll = 0, firstBad = -1, badA = 0, badB = 0;
	for (int t = 0; t < 65536; ++t) {
		int first = -1, seenBy = 0;
		for (int i = 0; i < numGbas; ++i) {
			if (!gba[i].seen[t]) {
				continue;
			}
			++seenBy;
			if (first < 0) {
				first = i;
			} else if (firstBad < 0 && gba[first].hashes[t * (numSyms + 1)] != gba[i].hashes[t * (numSyms + 1)]) {
				firstBad = t;
				badA = first;
				badB = i;
			}
		}
		if (seenBy > 1) {
			++compared;
		}
		if (seenBy && seenBy < numGbas) {
			++notAll;
		}
	}
	for (int i = 0; i < numGbas; ++i) {
		printf("gba%d: %d frames, %d game frames in %d video frames\n", i, gba[i].frames, gba[i].gameFramesDone,
		       gba[i].lastGameFrame - gba[i].firstGameFrame + 1);
		printf("gba%d: game frames took 1 video frame %d times, 2: %d, 3: %d, more (banners): %d\n", i,
		       gba[i].gaps[0], gba[i].gaps[1], gba[i].gaps[2], gba[i].gaps[3]);
	}
	printf("compared %d game frames (%d not seen by all)\n", compared, notAll);
	if (firstBad >= 0) {
		printf("DESYNC at game frame %d between gba%d and gba%d in:", firstBad, badA, badB);
		for (int i = 0; i < numSyms; ++i) {
			if (gba[badA].hashes[firstBad * (numSyms + 1) + i + 1] != gba[badB].hashes[firstBad * (numSyms + 1) + i + 1]) {
				printf(" %s", syms[i].name);
			}
		}
		printf("\n");
		for (int i = 0; i < 4 * PLAYER_SIZE; ++i) {
			uint8_t a = gba[badA].players[firstBad * 4 * PLAYER_SIZE + i];
			uint8_t b = gba[badB].players[firstBad * 4 * PLAYER_SIZE + i];
			if (a != b) {
				printf("  player[%d] byte %d: %02x vs %02x\n", i / PLAYER_SIZE, i % PLAYER_SIZE, a, b);
			}
		}
		return 1;
	}
	if (!compared) {
		printf("NOTHING COMPARED, the game never started\n");
		return 1;
	}
	printf("IN SYNC\n");
	return 0;
}
