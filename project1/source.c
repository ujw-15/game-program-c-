#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define H 14
#define W 33

#define UI_W 160
#define UI_H 45

#define M_X 41
#define M_Y 4
#define M_W 70
#define M_H 18

#define MSG_X 41
#define MSG_Y (M_H + M_Y + 2)
#define MSG_W 70
#define MSG_H 7

#define STAT_X 3
#define STAT_Y 3
#define STAT_W 25
#define STAT_H 10

#define DECK_BOX_X 3
#define DECK_BOX_Y 16
#define DECK_BOX_W 25
#define DECK_BOX_H 16

#define CARD_UI_X (UI_W - 30)
#define CARD_UI_Y 4

#define CODEX_COLS 6
#define CODEX_ROWS 3
#define CODEX_SLOTS (CODEX_COLS * CODEX_ROWS) // 18

#define SLOT_W 22
#define SLOT_H 8

#define CODEX_X 6
#define CODEX_Y 4
#define SLOT_GAP_X 3
#define SLOT_GAP_Y 1

#define FLOORS 3
#define SKILLS_PER_FLOOR 18
#define MAX_SKILLS (FLOORS * SKILLS_PER_FLOOR) // 54

#define MAX_EQUIP 6
#define ENERGY_MAX 3


#define STORE_BOX_X 3
#define STORE_BOX_Y 16
#define STORE_BOX_W 25
#define STORE_BOX_H 16

// printf/putchar를 백버퍼 출력으로 대체
#define printf  DB_printf
#define putchar DB_putchar

int moveCountSinceLastBattle = 0;

// ---------------------------
// 더블버퍼(CHAR_INFO) 기반 렌더
// ---------------------------
static HANDLE gCon = NULL;

static int gUIW = UI_W;
static int gUIH = UI_H;

static int gWinW = UI_W;
static int gWinH = UI_H;

static int gOffX = 0;
static int gOffY = 0;

static int gBufW = UI_W;
static int gBufH = UI_H;

static CHAR_INFO* gBack = NULL; // 메모리 백버퍼

static int gCurX = 0, gCurY = 0; // 버퍼 커서
static WORD gAttr = 0;
static void DB_ClearAll(void)
{
    if (!gBack) return;
    for (int i = 0; i < gBufW * gBufH; i++) {
        gBack[i].Char.AsciiChar = ' ';
        gBack[i].Attributes = gAttr;
    }
    gCurX = 0;
    gCurY = 0;
}
static void DB_UpdateWindowInfo(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(gCon, &csbi);
    gAttr = csbi.wAttributes;

    gWinW = (csbi.srWindow.Right - csbi.srWindow.Left + 1);
    gWinH = (csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    // 버퍼는 윈도우 크기 이상으로 확보(스크롤바/잘림 방지)
    int needW = (gWinW > gUIW) ? gWinW : gUIW;
    int needH = (gWinH > gUIH) ? gWinH : gUIH;

    if (needW != gBufW || needH != gBufH || gBack == NULL) {
        gBufW = needW;
        gBufH = needH;
        if (gBack) free(gBack);
        gBack = (CHAR_INFO*)malloc(sizeof(CHAR_INFO) * gBufW * gBufH);
    }

    // 중앙 오프셋 계산
    gOffX = (gWinW > gUIW) ? (gWinW - gUIW) / 2 : 0;
    gOffY = (gWinH > gUIH) ? (gWinH - gUIH) / 2 : 0;
    if (gOffX < 0) gOffX = 0;
    if (gOffY < 0) gOffY = 0;
}

void DB_Init(void)
{
    gCon = GetStdHandle(STD_OUTPUT_HANDLE);

    // 커서 숨김(윈도우 핸들 기반)
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(gCon, &info);
    info.bVisible = FALSE;
    SetConsoleCursorInfo(gCon, &info);

    DB_UpdateWindowInfo();
}

// 프레임 시작(원문에 호출이 있어서 추가)
void DB_BeginFrame(void)
{
    DB_UpdateWindowInfo();
    DB_ClearAll();
}

// 백버퍼 특정 좌표에 문자 찍기(화면 밖이면 무시)
static void DB_PutCharAt(int x, int y, char c)
{
    if (x < 0 || y < 0 || x >= gBufW || y >= gBufH) return;
    int idx = y * gBufW + x;
    gBack[idx].Char.AsciiChar = c;
    gBack[idx].Attributes = gAttr;
}

// 문자열 쓰기(자동 줄바꿈 최소 처리)
static void DB_WriteRaw(const char* s)
{
    if (!s) return;
    while (*s) {
        char c = *s++;

        if (c == '\n') {
            gCurX = 0;
            gCurY++;
            continue;
        }
        if (c == '\r') {
            gCurX = 0;
            continue;
        }

        int tx = gCurX;
        int ty = gCurY;

        DB_PutCharAt(tx, ty, c);

        gCurX++;
        if (gCurX >= gBufW) {
            gCurX = 0;
            gCurY++;
        }
        if (gCurY >= gBufH) {
            gCurY = gBufH - 1;
        }
    }
}

// 백버퍼를 콘솔에 한 번에 출력
void DB_Present(void)
{
    if (!gBack) return;

    // 창 크기 정보 갱신(중앙 오프셋 포함)
    DB_UpdateWindowInfo();

    SMALL_RECT rect;
    rect.Left = 0;
    rect.Top = 0;
    rect.Right = (SHORT)(gWinW - 1);
    rect.Bottom = (SHORT)(gWinH - 1);

    COORD bufSize = { (SHORT)gBufW, (SHORT)gBufH };
    COORD bufPos = { 0, 0 };

    WriteConsoleOutputA(gCon, gBack, bufSize, bufPos, &rect);
}

int DB_putchar(int c)
{
    char s[2];
    s[0] = (char)c;
    s[1] = '\0';
    DB_WriteRaw(s);
    return c;
}

int DB_printf(const char* fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    DB_WriteRaw(tmp);
    return (int)strlen(tmp);
}

void gotoxy(int x, int y)
{
    // UI좌표 -> 백버퍼 좌표 (중앙 오프셋 적용)
    gCurX = x + gOffX;
    gCurY = y + gOffY;
}

// 입력 대기(딜레이 대신)
void wait_any_key()
{
    DB_Present();
    _getch();
}

// --------------------
// 방향/플레이어/적
// --------------------
typedef enum { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 } Dir;
typedef struct { int x, y; } Pos;

static const Pos JOURNAL_POS[FLOORS][3] = {
    // floor 0의 일지 3개 위치 (x,y)
    { {31,4}, {8,7}, {31,9} },
    // floor 1
    { {0,0}, {0,0}, {0,0} },
    // floor 2
    { {0,0}, {0,0}, {0,0} },
};

typedef struct {
    int x, y;
    int HP, maxHP;
    char name[50];
    Dir dir;
    int level;
    int exp;
    int expToNext;

    // 장착 슬롯 6개
    int equip[MAX_EQUIP];
    int equipCount;

    // 체크포인트
    int cpX, cpY;
    int hasCheckpoint;

    // 현재 층(0..2)
    int floor;

    int poison;
    int burn;
    int vuln;
    int weak;

    int gold;
    int altarGuardBonus;

    unsigned char journalMask[FLOORS];


} Player;

typedef struct {
    char name[32];
    char desc[80];
    int price;
    void (*apply)(Player*);
} PassiveItem;


int GetJournalIndex(int floor, int x, int y)
{
    for (int i = 0; i < 3; i++) {
        if (JOURNAL_POS[floor][i].x == x && JOURNAL_POS[floor][i].y == y)
            return i;
    }
    return -1;
}

typedef enum { EFF_NONE = 0, EFF_POISON, EFF_BURN, EFF_VULN, EFF_WEAK } EffectType;

typedef struct {
    char name[20];
    int hp;
    int maxHp;
    int atk;
    int intent;
    int expReward;
    int poison;
    int burn;
    int vuln;
    int weak;
} Enemy;

// 스킬(도감 DB 54개)
typedef struct {
    int id;
    char name[32];
    int cost;
    int dmg, block, heal;
    int unlocked;
    char dec[80];
    EffectType eff;
    int effVal;
} Skill;

Skill codexSkills[MAX_SKILLS];
int codexCount = MAX_SKILLS;

// 상태이상 처리
int CalcPlayerDamage(int base, Enemy* e, Player* p)
{
    int dmg = base;

    // 약화: 주는 피해 감소
    if (p->weak > 0) dmg = dmg * 75 / 100;

    // 취약: 받는 피해 증가 (원문 그대로 유지)
    if (e->vuln > 0) dmg = dmg * 150 / 100;

    // 독: 공격 시 추가 피해
    if (e->poison > 0) dmg += e->poison;

    if (dmg < 0) dmg = 0;
    return dmg;
}

// --------------------
// 일지 대사
// --------------------
const char* JOURNAL_TEXT[FLOORS][3][40] = {
    // ===== 1층 =====
    {
        {
            "아무것도 기억 나지 않는다.",
            "아무리 기억을 찾아보려 애써도 이 축축한 던전 밖에서의 기억이 하나도 나지않는다.",
            "그저 내가 밖에서 어떤 인물이었는지 짐작만이 가능하다",
            "추측건데 아마 나는 꽤나 유망한 검사였을 것이다.",
            "어떻게 아냐고?",
            "처음으로 이 던전에서 깨어나 그 불쾌하기 짝이없는 고블린의 목을 칠떄 느꼈을 뿐이다. '익숙하다'라고 느꼈다.",
            "던전에서 검을 처음 잡았을떄 내 손에선 익숙함이 물씬 품겼고, 몸은 자연스래 움직였다.",
            "뭐. 그렇기에 내가 지금 멀정히 이 글을 쓰고 있는 거겠지",
            "허나 이것이 끝이다. 앞서 말했듯 아무 기억도 나지 않는다.",
            "정말 아무것도, 아무것도 기억나지 않는다",
            "언제까지 이 잡몹들을 잡아야 되는데",
            "몸에선 이미 고블린 피 냄새가 진동을 해 미칠지경이다.",
            "장화 안에선 슬라임 살점이 질퍽거리고, 스켈레톤을 잡을때마다 안스러워진다.",
            "언젠가 내 꼴이 저 해골과 똑같이 변할 것 같으니까.",
            "난 왜 여기있는 걸까",
            NULL
        },
        {
            "이곳은 뭐하는 곳일까",
            "아무리 생각해도 해답은 나오지않는다.",
            "난 왜 여기있지?",
            "모르겠다",
            "전혀",
            "잡아도 잡아도 계속 튀어 나오는 이 몬스터들은 뭘까",
            "잘 모르겠다",
            "전혀",
            "이 던전 밖에서도 머리를 잘 쓰는편은 아니였나보다.",
            "뭐 아무튼 이 일지를 쓰기전에 한 상인과 마주쳤다.",
            "아무말도 없이 자기 물건을 가리키길래 그냥 주는건줄알고 들고 갈라니 잠시 정신을 읽었다.",
            "다시보니 이 던전내에서 나오는 골드란 거랑 거래하는 거였더라고.",
            "이 일지를 볼 사람이 또 있을지 모르겠지만 충고하나 하자면.",
            "이런곳에서 상인 행세를 하는걸보면 정상적인 사람은 아닌것 같은니",
            "되도록 물건만 얻고 빠르게 떠나길 바란다.",
            "마침",
            NULL
        },
        {
            "신이 있다면 이런곳은 왜 있는것일까",
            "그렇게 말해왔던 신은 왜 이럴때 아무 도움도 안되는거지",
            "전지전능하다면 그냥 날 밖으로 꺼내줄수도 있는거 아닌가?",
            "하소연할곳이 이 작은 종이 뿐이라니..",
            "지금의 내가 안쓰러워 미칠 지경이다",
            "전에 이곳을 돌아다니다 한 일지를 봤다.",
            "물론 그 일지도 자신의 신세를 한탄하는 그런 뻔한 글이 적혀있었다.",
            "기억이 나지 않는다느니, 괴물들 피 냄새가 진동을 한다느니,,뭐 그런 이런상황에서 쓸법한 뻔한 글",
            "그럼에도 얼마나 기쁘던지, 이 공간에 다른 사람이 있는게 아닐까? 하는 생각을 했다",
            "기뻣다.",
            "그래서 하소연도 할겸 해서 글을 쓰는거다",
            "다음번이 올지 모르겠지만 만약 온다면",
            "그떄의 나와 같은 감정을 느끼길 바라며",
            "이 글을 쓰고나서 난 저 앞에 있는 커다란 녀석과 싸울거다",
            "그 녀석 뒤에 어딘가로 올라갈수있는 장치가 있다.",
            "그 장소가 이 칙칙한 던전이 아니길 빌며",
            "...",
            NULL
        }
    },
    // ===== 2층 =====
    {
        { "2층 일지 1", NULL },
        { "2층 일지 2", NULL },
        { "2층 일지 3", NULL }
    },
    // ===== 3층 =====
    {
        { "3층 일지 1", NULL },
        { "3층 일지 2", NULL },
        { "3층 일지 3", NULL }
    }
};

// --------------------
// 콘솔 유틸
// --------------------
char dirchar(Dir d)
{
    switch (d) {
    case NORTH: return '^';
    case EAST:  return '>';
    case SOUTH: return 'v';
    case WEST:  return '<';
    }
    return '?';
}

void waitenter()
{
    int eh;
    while (1) {
        DB_Present();
        eh = _getch();
        if (eh == 13) break;
    }
}

void showStartScreen()
{
    system("cls");
    printf("\n\n");
    for (int i = 0; i < 160; i++) printf("=");
    printf("\n\n");
    printf(" (예제)DUNGEON GAME\n\n\n");
    for (int i = 0; i < 160; i++) printf("=");
    printf("\n\n");
    printf(" press enter key to start\n");
    waitenter();
}

void showNarration(Player* p)
{
    system("cls");
    printf("\n");
    printf("====================================\n");
    printf(" NARRATION \n");
    printf("====================================\n\n");
    printf("[Enter] 다음\n\n");
    waitenter();

    printf("...눈을 뜨자, 낯선 공기가 폐를 찔렀다.\n\n");
    waitenter();

    printf("벽은 차갑고, 바닥은 젖어 있었다, 앞은 칠흙같은 어둠으로 번져있다.\n\n");
    waitenter();

    printf("어딘가에서, 아주 작은 숨소리가 들린다.\n\n");
    waitenter();

    printf("당신은 그 나약한 숨소리가 본인이라는 것을 인지했다.");
    waitenter();

    system("cls");
    printf("\n");
    printf("====================================\n");
    printf(" NARRATION \n");
    printf("====================================\n\n");
    printf("당신은 무언가를 잊고 있다.\n\n");
    waitenter();

    printf("이곳에 들어온 이유도, 돌아갈 길도.\n\n");
    waitenter();

    printf("하지만 한 가지는 확실하다.\n\n");
    waitenter();

    printf("여기서 나가야 한다는 것을.\n\n");
    waitenter();

    system("cls");
    printf("\n");
    printf("====================================\n");
    printf(" NARRATION \n");
    printf("====================================\n\n");
    printf("당신은 몸을 쑤셔오는 고통과 공포를 뒤엎고 일어셨다.\n\n");
    waitenter();

    printf("당신은 희미해져가는 머리속에서 자신의 이름을 생각한다.\n\n");
    waitenter();

    system("cls");
    printf("내 이름은....\n\n");
    printf("당신의 이름을 생각하시오: \n\n");

    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
    fgets((*p).name, sizeof((*p).name), stdin);
    (*p).name[strcspn((*p).name, "\n")] = '\0';
    if ((*p).name[0] == '\0') strcpy_s((*p).name, sizeof((*p).name), "방랑자");
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

    printf("어지러운 머리속에선 이름만이 기억난다.\n");
    waitenter();

    system("cls");
    printf("\n");
    printf("====================================\n");
    printf(" NARRATION \n");
    printf("====================================\n\n");
    printf("%s은(는) 근처 상황을 살펴본다.\n\n", (*p).name);
    waitenter();

    printf("보란듯이 놓여져있는 낡은 검과 방패,,\n\n");
    waitenter();

    printf("%s은(는) 꺼림찍함을 뒤로하고 검과 방패를 장비한다,\n\n", (*p).name);
    waitenter();

    printf("%s은(는) 마음을 한차례 심호흡을 한 후 커다란 문을 열고 앞으로 나아간다.", (*p).name);
    waitenter();
}

void showCursor(int show)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &info);
}

void setConsoleSize(int width, int height)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufferSize = { (SHORT)width, (SHORT)height };
    SetConsoleScreenBufferSize(h, bufferSize);
    SMALL_RECT windowSize = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
    SetConsoleWindowInfo(h, TRUE, &windowSize);
}

void drawbox(int x, int y, int w, int h, const char* title)
{
    gotoxy(x, y);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r);
        putchar('|');
        gotoxy(x + w - 1, y + r);
        putchar('|');
    }

    gotoxy(x, y + h - 1);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    if (title && title[0]) {
        gotoxy(x + 2, y);
        printf("%s", title);
    }
}

void clearBoxInner(int x, int y, int w, int h)
{
    for (int r = 1; r < h - 1; r++) {
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
    }
}

void ClearPendingInputSafe()
{
    while (_kbhit()) {
        int c = _getch();
        if (c == 224 || c == 0) {
            if (_kbhit()) _getch();
        }
    }
}

void ClearScreenNoFlicker()
{
    // system("cls")를 피하려고 만든 함수였던 듯.
    // 실제 지우기는 백버퍼로 처리중이므로 여기서는 큰 의미 없지만 유지.
    DB_BeginFrame();
}

void clearArea(int x, int y, int w, int h)
{
    for (int r = 0; r < h; r++) {
        gotoxy(x, y + r);
        for (int i = 0; i < w; i++) putchar(' ');
    }
}

void drawUIFrame()
{
    ClearScreenNoFlicker();

    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(M_X, M_Y, M_W, M_H, "");
    drawbox(MSG_X, MSG_Y, MSG_W, MSG_H, " 메세지 ");
    drawbox(STAT_X, STAT_Y, STAT_W, STAT_H, " 상태 ");
    drawbox(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H, " 스킬북 ");

    clearBoxInner(M_X, M_Y, M_W, M_H);
    clearBoxInner(MSG_X, MSG_Y, MSG_W, MSG_H);
    clearBoxInner(STAT_X, STAT_Y, STAT_W, STAT_H);
    clearBoxInner(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H);


}

void renderMessageLine(int line, const char* msg)
{
    int y = MSG_Y + 2 + line;
    gotoxy(MSG_X + 2, y);
    printf("%-66s", msg ? msg : "");
}

void renderMessage(const char* msg)
{
    renderMessageLine(0, msg);
}

void CodexNotice(const char* msg)
{
    int x = CODEX_X;
    int y = CODEX_Y + CODEX_ROWS * SLOT_H + (CODEX_ROWS - 1) * SLOT_GAP_Y + 1;
    if (y > UI_H - 3) y = UI_H - 3;

    gotoxy(x, y);
    printf("%-150s", "");
    gotoxy(x, y);
    printf("%-150s", msg ? msg : "");
}

void playerstate(Player* p)
{
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%-22s", "");
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%s", p->name);

    gotoxy(STAT_X + 2, STAT_Y + 3);
    printf("레벨 %d", p->level);

    gotoxy(STAT_X + 2, STAT_Y + 4);
    printf("경험치 %d/%d", p->exp, p->expToNext);

    gotoxy(STAT_X + 2, STAT_Y + 5);
    printf("체력 %d/%d ", p->HP, p->maxHP);

    gotoxy(STAT_X + 2, STAT_Y + 6);
    printf("골드 %d ", p->gold);

    gotoxy(STAT_X + 2, STAT_Y + 7);
    printf("층 %d ", p->floor + 1);

    gotoxy(STAT_X + 2, STAT_Y + 8);
    if (p->hasCheckpoint) printf("체크포인트 O ");
    else printf("체크포인트 X ");
}

void renderSkillBookObject()
{
    int x = DECK_BOX_X + 3;
    int y = DECK_BOX_Y + 2;

    gotoxy(x, y + 0);  printf(" ______________");
    gotoxy(x, y + 1);  printf(" /___________/||");
    gotoxy(x, y + 2);  printf(" +------------+||");
    gotoxy(x, y + 3);  printf(" |            |||");
    gotoxy(x, y + 4);  printf(" |  스 킬 북  |||");
    gotoxy(x, y + 5);  printf(" |            |||");
    gotoxy(x, y + 6);  printf(" |            |||");
    gotoxy(x, y + 7);  printf(" |            |||");
    gotoxy(x, y + 8);  printf(" |            |||");
    gotoxy(x, y + 9);  printf(" |            |||");
    gotoxy(x, y + 10); printf(" +------------+/");
}

// 상인이 판매하는 패시브 아이템 수 : 
//void Spassiveitems(PassiveItem*a)
//{
//    
//}
//
//void store()
//{
//    DB_BeginFrame();
//
//    drawbox(10, 5, 140, 30, " 상점 ");
// 
//    
//
//
//}

// --------------------
// 장착(6개) 유틸
// --------------------
int IsEquipped(Player* p, int skillId)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) return 1;
    }
    return 0;
}

int FirstEmptyEquipSlot(Player* p)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == -1) return i;
    }
    return -1;
}

void RecountEquip(Player* p)
{
    int c = 0;
    for (int i = 0; i < MAX_EQUIP; i++)
        if (p->equip[i] != -1) c++;
    p->equipCount = c;
}

void UnequipSkill(Player* p, int skillId)
{
    for (int i = 0; i < MAX_EQUIP; i++) {
        if (p->equip[i] == skillId) {
            p->equip[i] = -1;
            break;
        }
    }
    RecountEquip(p);
}

void EquipSkill(Player* p, int skillId)
{
    if (IsEquipped(p, skillId)) return;

    int slot = FirstEmptyEquipSlot(p);
    if (slot != -1) {
        p->equip[slot] = skillId;
        RecountEquip(p);
        return;
    }

    // 꽉 차 있으면 교체 선택 UI (1~6)
    gotoxy(34, 31);
    printf("%-80s", ""); // 잔상 지우기용(폭 적당히)
    gotoxy(34, 31);
    printf("슬롯이 가득 찼다. 교체할 슬롯(1~6) 입력, ESC 취소");

    DB_Present();
    int ch = _getch();
    if (ch == 27) return;

    if (ch >= '1' && ch <= '6') {
        int idx = ch - '1';
        p->equip[idx] = skillId;
        RecountEquip(p);
    }
}

void DrawEquippedList(Player* p, int x, int y)
{
    gotoxy(x + 4, y + 15);
    printf("장착(전투 사용) 스킬 6개:");
    for (int i = 0; i < MAX_EQUIP; i++) {
        gotoxy(x, y + 1 + i);
        if (p->equip[i] == -1) {
            printf("%d) (비어있음) ", i + 1);
        }
        else {
            Skill* s = &codexSkills[p->equip[i]];
            printf("%d) %-18s(비용:%d) ", i + 1, s->name, s->cost);
        }
    }
    gotoxy(x, y + 8);
    printf("아무 키: 닫기");
}

// --------------------
// 도감
// --------------------
void ShowSkillDetail(Skill s)
{
    ClearScreenNoFlicker();
    drawbox(20, 5, 120, 30, " 스킬 상세 ");

    if (!s.unlocked) {
        gotoxy(24, 10);
        printf("잠김 상태");
        gotoxy(24, 11);
        printf("해금이 필요합니다.");
        gotoxy(24, 28);
        printf("아무 키: 돌아가기");
        wait_any_key();
        return;
    }

    gotoxy(24, 9);  printf("이름 : %s", s.name);
    gotoxy(24, 11); printf("비용 : %d", s.cost);
    gotoxy(24, 13); printf("피해 : %d", s.dmg);
    gotoxy(24, 14); printf("방어 : %d", s.block);
    gotoxy(24, 15); printf("회복 : %d", s.heal);
    gotoxy(24, 18); printf("설명 : %s", s.dec);

    gotoxy(24, 28);
    printf("아무 키: 돌아가기");
    wait_any_key();
}

void DrawCodexSlot(int x, int y, int w, int h, const Skill* s, int hasSkill, int selected, int equipped)
{
    // 테두리
    gotoxy(x, y);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r);
        putchar('|');
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
        gotoxy(x + w - 1, y + r);
        putchar('|');
    }

    gotoxy(x, y + h - 1);
    putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    // 선택표시(잔상 제거 포함)
    gotoxy(x - 2, y + h / 2);
    printf("%c", selected ? '>' : ' ');
    gotoxy(x + w + 1, y + h / 2);
    printf("%c", selected ? '<' : ' ');

    // 내용
    if (!hasSkill || !s) {
        gotoxy(x + 2, y + 2);
        printf("%-18s", "(EMPTY)");
        gotoxy(x + 2, y + 6);
        printf("%-6s", " ");
        return;
    }

    if (!s->unlocked) {
        gotoxy(x + 2, y + 2);
        printf("%-18s", "???? (잠김)");
        gotoxy(x + 2, y + 4);
        printf("%-18s", "해금 필요");
        gotoxy(x + 2, y + 6);
        printf("%-6s", " ");
        return;
    }

    gotoxy(x + 2, y + 2); printf("%-18.18s", s->name);
    gotoxy(x + 2, y + 3); printf("비용:%-2d ", s->cost);
    gotoxy(x + 2, y + 4); printf("피:%-3d 방:%-3d ", s->dmg, s->block);
    gotoxy(x + 2, y + 5); printf("회:%-3d ", s->heal);

    gotoxy(x + 2, y + 6);
    printf("%-6s", equipped ? "[장착]" : " ");
}

static void RedrawOneCodexSlot(Player* p, int floorPage, int slotIdx, int selectedSlot)
{
    int startIndex = floorPage * SKILLS_PER_FLOOR;
    int skillIdx = startIndex + slotIdx;

    int x = CODEX_X + (slotIdx % CODEX_COLS) * (SLOT_W + SLOT_GAP_X);
    int y = CODEX_Y + (slotIdx / CODEX_COLS) * (SLOT_H + SLOT_GAP_Y);

    int hasSkill = (skillIdx >= 0 && skillIdx < MAX_SKILLS);
    int equipped = 0;

    if (hasSkill && codexSkills[skillIdx].unlocked)
        equipped = IsEquipped(p, codexSkills[skillIdx].id);

    if (hasSkill)
        DrawCodexSlot(x, y, SLOT_W, SLOT_H, &codexSkills[skillIdx], 1, (slotIdx == selectedSlot), equipped);
    else
        DrawCodexSlot(x, y, SLOT_W, SLOT_H, NULL, 0, (slotIdx == selectedSlot), 0);
}

static void RedrawCodexPage(Player* p, int floorPage, int selected)
{
    clearBoxInner(2, 1, UI_W - 4, UI_H - 2);
    gotoxy(6, 2);
    printf("E/ESC: 닫기 | ENTER: 상세 | R: 장착/해제 | T: 장착목록 | 방향키: 이동 | A/D: 층 페이지 (층 %d) ",
        floorPage + 1);

    for (int i = 0; i < CODEX_SLOTS; i++) {
        RedrawOneCodexSlot(p, floorPage, i, selected);
    }
    CodexNotice(" ");
}

void OpenCodexScene(Player* p)
{
    int floorPage = p->floor;
    int selected = 0;

    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
    RedrawCodexPage(p, floorPage, selected);
    DB_Present();

    while (1) {
        int ch = _getch();

        if (ch == 224 || ch == 0) {
            int k = _getch();
            int prev = selected;

            int row = selected / CODEX_COLS;
            int col = selected % CODEX_COLS;

            if (k == 72 && row > 0) row--;
            else if (k == 80 && row < CODEX_ROWS - 1) row++;
            else if (k == 75 && col > 0) col--;
            else if (k == 77 && col < CODEX_COLS - 1) col++;

            selected = row * CODEX_COLS + col;

            if (selected != prev) {
                RedrawOneCodexSlot(p, floorPage, prev, selected);
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                CodexNotice(" ");
                DB_Present();
            }
            continue;
        }

        if (ch == 27 || ch == 'e' || ch == 'E') {
            ClearPendingInputSafe();
            return;
        }

        if (ch == 'a' || ch == 'A') {
            if (floorPage > 0) floorPage--;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }
        if (ch == 'd' || ch == 'D') {
            if (floorPage < FLOORS - 1) floorPage++;
            selected = 0;
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 't' || ch == 'T') {
            DB_BeginFrame();
            drawbox(10, 5, 140, 30, " 장착 목록 ");
            DrawEquippedList(p, 14, 8);
            wait_any_key();

            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 13) {
            int idx = floorPage * SKILLS_PER_FLOOR + selected;
            if (idx >= 0 && idx < MAX_SKILLS) ShowSkillDetail(codexSkills[idx]);

            drawbox(0, 0, UI_W, UI_H, "");
            drawbox(2, 1, UI_W - 4, UI_H - 2, " 도감 ");
            RedrawCodexPage(p, floorPage, selected);
            DB_Present();
            continue;
        }

        if (ch == 'r' || ch == 'R') {
            int idx = floorPage * SKILLS_PER_FLOOR + selected;
            if (idx >= 0 && idx < MAX_SKILLS) {
                Skill* s = &codexSkills[idx];
                if (!s->unlocked) {
                    CodexNotice("잠김 스킬은 장착할 수 없다. 아무 키.");
                    wait_any_key();
                }
                else {
                    if (IsEquipped(p, s->id)) {
                        UnequipSkill(p, s->id);
                        CodexNotice("장착 해제됨. 아무 키.");
                    }
                    else {
                        EquipSkill(p, s->id);
                        CodexNotice("장착(또는 교체) 완료. 아무 키.");
                    }
                    wait_any_key();
                }
                RedrawOneCodexSlot(p, floorPage, selected, selected);
                CodexNotice(" ");
                DB_Present();
            }
            continue;
        }
    }
}

// --------------------
// 스킬 DB 초기화
// --------------------
void SetSkill(int idx, const char* name, int cost, int dmg, int block, int heal, int unlocked,
    const char* dec, EffectType eff, int effVal)
{
    codexSkills[idx].id = idx;
    strcpy_s(codexSkills[idx].name, sizeof(codexSkills[idx].name), name);
    strcpy_s(codexSkills[idx].dec, sizeof(codexSkills[idx].dec), dec);
    codexSkills[idx].cost = cost;
    codexSkills[idx].dmg = dmg;
    codexSkills[idx].block = block;
    codexSkills[idx].heal = heal;
    codexSkills[idx].unlocked = unlocked;
    codexSkills[idx].eff = eff;
    codexSkills[idx].effVal = effVal;
}

void ApplyTurnEnd_Player(Player* p)
{
    if (p->burn > 0) {
        p->HP -= p->burn;
        if (p->HP < 0) p->HP = 0;
        p->burn--;
    }
    if (p->vuln > 0) p->vuln--;
    if (p->weak > 0) p->weak--;
}

void ApplyTurnEnd_Enemy(Enemy* e)
{
    if (e->burn > 0) {
        e->hp -= e->burn;
        if (e->hp < 0) e->hp = 0;
        e->burn--;
    }
    if (e->vuln > 0) e->vuln--;
    if (e->weak > 0) e->weak--;
}

void InitCodex()
{
    for (int i = 0; i < MAX_SKILLS; i++) {
        SetSkill(i, "미정", 99, 0, 0, 0, 0, "아직 없음", EFF_NONE, 0);
    }

    // ====== 층1 스킬 18개 (0~17) ======
    SetSkill(0, "공격", 1, 100, 0, 0, 1, "기본 공격", EFF_NONE, 0);
    SetSkill(1, "방어", 1, 0, 6, 0, 1, "기본 방어", EFF_NONE, 0);
    SetSkill(2, "회복", 2, 0, 0, 10, 1, "기본 회복", EFF_NONE, 0);

    SetSkill(3, "속공", 0, 6, 0, 0, 0, "0비용 빠른 공격", EFF_NONE, 0);
    SetSkill(4, "찌르기", 1, 8, 0, 0, 0, "독 상태 적에게 추가 피해", EFF_NONE, 0);
    SetSkill(5, "강타", 2, 18, 0, 0, 0, "취약 적에게 추가 피해", EFF_NONE, 0);

    SetSkill(6, "독 바르기", 1, 4, 0, 0, 0, "독 3 부여", EFF_POISON, 3);
    SetSkill(7, "침식", 1, 0, 0, 0, 0, "독 5 부여", EFF_POISON, 5);
    SetSkill(8, "신경 독소", 2, 6, 0, 0, 0, "독 4 부여", EFF_POISON, 4);

    SetSkill(9, "화염 참격", 2, 14, 0, 0, 0, "화상 3 부여", EFF_BURN, 3);
    SetSkill(10, "불씨", 1, 4, 0, 0, 0, "화상 3 부여", EFF_BURN, 3);
    SetSkill(11, "잔불", 0, 0, 0, 0, 0, "0비용 화상 1", EFF_BURN, 1);

    SetSkill(12, "약점 노출", 1, 0, 0, 0, 0, "취약 2턴", EFF_VULN, 2);
    SetSkill(13, "기력 붕괴", 1, 6, 0, 0, 0, "약화 2턴", EFF_WEAK, 2);
    SetSkill(14, "압박", 2, 0, 0, 0, 0, "취약 3턴", EFF_VULN, 3);

    SetSkill(15, "집중", 2, 0, 20, 0, 0, "안정적인 방어", EFF_NONE, 0);
    SetSkill(16, "철벽", 2, 10, 16, 0, 0, "최선의 방어는 공격", EFF_NONE, 0);
    SetSkill(17, "흡혈", 2, 10, 0, 15, 0, "공격 후 회복", EFF_NONE, 0);
}

// --------------------
// 해금(상자/상인/처치) 유틸
// --------------------
int RandomLockedSkillOnFloor(int floor)
{
    int start = floor * SKILLS_PER_FLOOR;
    int tries = 200;

    while (tries--) {
        int idx = start + (rand() % SKILLS_PER_FLOOR);
        if (idx >= 0 && idx < MAX_SKILLS && codexSkills[idx].unlocked == 0)
            return idx;
    }
    return -1;
}

void UnlockOneSkill(Player* p, int floor)
{
    int idx = RandomLockedSkillOnFloor(floor);
    if (idx == -1) {
        renderMessage("이 층의 스킬은 이미 전부 해금됨. 아무 키.");
        wait_any_key();
        return;
    }

    codexSkills[idx].unlocked = 1;
    char buf[160];
    sprintf_s(buf, sizeof(buf),
        "스킬 해금: %s (도감에 기록됨) [E]=도감 열기 / 아무키=계속",
        codexSkills[idx].name);

    renderMessage(buf);
    wait_any_key();
}

// --------------------
// 일지
// --------------------
void DrawJournal(int floor, int jIndex)
{
    ClearScreenNoFlicker();
    drawbox(20, 5, 120, 30, " 일지 ");

    int x = 24;
    int y = 9;

    for (int i = 0; i < 40; i++) {
        const char* line = JOURNAL_TEXT[floor][jIndex][i];
        if (!line) break;
        gotoxy(x, y + i);
        printf("%-100s", line);
    }


    gotoxy(24, 28);
    printf("아무 키: 닫기");
    wait_any_key();
}

void HandleJournalEvent(Player* p, int floor, int jIndex)
{
    if (p->journalMask[floor] & (1 << jIndex)) {
        renderMessage("이미 읽은 일지다.");
        wait_any_key();
        return;
    }

    p->journalMask[floor] |= (1 << jIndex);
    p->gold += 30;
    char buf[128];
    sprintf_s(buf, sizeof(buf), "일지와 함께 소량의 골드를 발견했다! 골드 +30. 아무 키.");
    renderMessage(buf);
    wait_any_key();


    DrawJournal(floor, jIndex);
}

int CountReadJournals(Player* p, int floor)
{
    int cnt = 0;
    for (int i = 0; i < 3; i++) {
        if (p->journalMask[floor] & (1u << i)) cnt++;
    }
    return cnt;
}

// --------------------
// 제단
// --------------------
void AltarEvent(Player* p)
{
    DB_BeginFrame();

    drawbox(10, 5, 140, 30, " 제단 ");

    gotoxy(14, 8);
    printf("붉은 제단이 네게 네 가지 선택을 제시한다.");

    gotoxy(14, 10);
    printf("1) 피의 계약 : 현재 체력 -30 → 이 층 스킬 2개 해금");

    gotoxy(14, 12);
    printf("2) 금지된 지식 : 골드 40 소모 → 다음 층 스킬 1개 해금");

    gotoxy(14, 14);
    printf("3) 저주받은 축복 : 약화 3턴 → 스킬 1개 해금 + 방어막 12");

    gotoxy(14, 16);
    printf("4) 운명의 도박 : 50%% 성공(스킬 2개) / 50%% 실패(체력-20, 약화2)");

    gotoxy(14, 18);
    printf("0) 아무것도 하지 않는다");
    DB_Present();
    int ch = _getch();

    if (ch == '0' || ch == 27) {
        renderMessage("아무 일도 일어나지 않았다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '1') {
        int loss = 30;
        if (p->HP <= loss) loss = p->HP - 1;
        if (loss < 0) loss = 0;
        p->HP -= loss;

        UnlockOneSkill(p, p->floor);
        UnlockOneSkill(p, p->floor);

        renderMessage("피의 계약을 맺었다. 힘을 얻었지만 피를 흘렸다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '2') {
        if (p->gold < 60) {
            renderMessage("골드가 부족하다. (아무 키)");
            wait_any_key();
            return;
        }

        p->gold -= 60;

        int nextFloor = p->floor + 1;
        if (nextFloor >= FLOORS) nextFloor = FLOORS - 1;

        UnlockOneSkill(p, nextFloor);

        renderMessage("경험하지 못한 지식을 손에 넣었다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '3') {
        p->weak += 3;
        p->altarGuardBonus += 12;
        UnlockOneSkill(p, p->floor);

        renderMessage("저주와 축복이 동시에 내렸다. (아무 키)");
        wait_any_key();
        return;
    }

    if (ch == '4') {
        if ((rand() % 100) < 50) {
            UnlockOneSkill(p, p->floor);
            UnlockOneSkill(p, p->floor);
            renderMessage("대성공! 지식이 폭발적으로 열렸다! (아무 키)");
        }
        else {
            int loss = 50;
            if (p->HP <= loss) loss = p->HP - 1;
            if (loss < 0) loss = 0;
            p->HP -= loss;
            p->weak += 2;
            renderMessage("실패… 제단이 네 생명을 앗아갔다. (아무 키)");
        }
        wait_any_key();
        return;
    }
}

// --------------------
// 적 생성
// --------------------
Enemy makeEnemyForFloor(int floor)
{
    Enemy e;
    memset(&e, 0, sizeof(e));
    int type = rand() % 5;

    if (floor == 0) {
        const char* names[5] = { "슬라임", "박쥐", "고블린", "해골", "거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 35 + type * 5;
        e.atk = 6 + type;
        e.expReward = 6 + type;
    }
    else if (floor == 1) {
        const char* names[5] = { "강슬라임", "늑대", "망령", "전사해골", "독거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 55 + type * 7;
        e.atk = 9 + type * 2;
        e.expReward = 10 + type;
    }
    else {
        const char* names[5] = { "흑기사", "거대박쥐", "저주망령", "암흑전사", "독왕거미" };
        strcpy_s(e.name, sizeof(e.name), names[type]);
        e.maxHp = 80 + type * 10;
        e.atk = 13 + type * 3;
        e.expReward = 16 + type * 2;
    }

    e.hp = e.maxHp;
    e.intent = 0;
    return e;
}

// --------------------
// 경험치/레벨업
// --------------------
void gainExp(Player* p, int amount)
{
    p->exp += amount;
    while (p->exp >= p->expToNext) {
        p->exp -= p->expToNext;
        p->level++;

        p->maxHP += 10;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
        p->expToNext += 5;

        renderMessage("레벨업! 최대 체력 +10. 아무 키.");
        wait_any_key();
    }
}

// --------------------
// 전투 UI (오른쪽에 장착 6개 스킬 리스트)
// --------------------
void drawSkillCardMini(int x, int y, const Skill* s, int selected)
{
    gotoxy(x, y);     printf("+----------------------+");
    gotoxy(x, y + 1); printf("|                      |");
    gotoxy(x, y + 2); printf("|                      |");
    gotoxy(x, y + 3); printf("|                      |");
    gotoxy(x, y + 4); printf("+----------------------+");

    if (selected) {
        gotoxy(x - 2, y + 2); printf(">>");
        gotoxy(x + 24, y + 2); printf("<<");
    }

    if (!s) {
        gotoxy(x + 2, y + 1);
        printf("(비어있음)");
        return;
    }

    gotoxy(x + 2, y + 1);  printf("%-16s", s->name);
    gotoxy(x + 19, y + 1); printf("비용:%d", s->cost);
    gotoxy(x + 2, y + 2);  printf("피:%2d 방:%2d 회:%2d", s->dmg, s->block, s->heal);
    gotoxy(x + 2, y + 3);  printf("%-20.20s", s->dec);
}

void drawEquippedSkillsUI(Player* p, int selected)
{
    int baseX = CARD_UI_X;
    int baseY = CARD_UI_Y;
    int gapY = 5;

    for (int i = 0; i < MAX_EQUIP; i++) {
        Skill* s = NULL;
        if (p->equip[i] != -1) s = &codexSkills[p->equip[i]];
        drawSkillCardMini(baseX, baseY + i * gapY, s, (i == selected));
    }
}

// --------------------
// 전투 로직
// --------------------
int applySkill(Player* p, Enemy* e, int* block, int* energy, Skill* s)
{
    if (!s) {
        renderMessage("비어있는 슬롯이다. 아무 키.");
        return 0;
    }
    if (*energy < s->cost) {
        renderMessage("에너지가 부족하다. 아무 키.");
        return 0;
    }

    *energy -= s->cost;

    if (s->dmg > 0) {
        int base = s->dmg;

        if (s->id == 4 && e->poison > 0) base += 4;  // 찌르기
        if (s->id == 5 && e->vuln > 0)   base += 10; // 강타

        int real = CalcPlayerDamage(base, e, p);
        e->hp -= real;
        if (e->hp < 0) e->hp = 0;
    }

    if (s->block > 0) *block += s->block;

    if (s->heal > 0) {
        p->HP += s->heal;
        if (p->HP > p->maxHP) p->HP = p->maxHP;
    }

    switch (s->eff) {
    case EFF_POISON: e->poison += s->effVal; break;
    case EFF_BURN:   e->burn += s->effVal; break;
    case EFF_VULN:   e->vuln += s->effVal; break;
    case EFF_WEAK:   e->weak += s->effVal; break;
    default: break;
    }

    char buf[160];
    sprintf_s(buf, sizeof(buf), "사용: %s | 남은에너지:%d | 적체력:%d/%d (아무 키)",
        s->name, *energy, e->hp, e->maxHp);
    renderMessage(buf);
    wait_any_key();

    return 1;
}

void enemyTurn(Player* p, Enemy* e, int* block)
{
    e->intent = rand() % 3; // 0 공격, 1 강공, 2 방어

    if (e->intent == 2) {
        renderMessage("적이 수비 자세를 취한다. 아무 키.");
        wait_any_key();
        return;
    }

    int dmg = e->atk + (e->intent == 1 ? 5 : 0);
    int taken = dmg;

    if (*block > 0) {
        int used = (*block >= taken) ? taken : *block;
        *block -= used;
        taken -= used;
    }

    if (taken > 0) {
        p->HP -= taken;
        if (p->HP < 0) p->HP = 0;
    }

    char buf[128];
    if (e->intent == 1)
        sprintf_s(buf, sizeof(buf), "%s의 강공! 피해:%d (아무 키)", e->name, dmg);
    else
        sprintf_s(buf, sizeof(buf), "%s의 공격! 피해:%d (아무 키)", e->name, dmg);

    renderMessage(buf);
    wait_any_key();

    *block = 0;
}

void RespawnAtCheckpoint(Player* p)
{
    if (p->hasCheckpoint) {
        p->x = p->cpX;
        p->y = p->cpY;
    }
    else {
        p->x = 1;
        p->y = 1;
    }
    p->HP = p->maxHP;
}

int battleLoop(Player* p)
{
    Enemy enemy = makeEnemyForFloor(p->floor);

    int energy = ENERGY_MAX;
    int block = 0;
    int selected = 0;
    int zeroUsedThisTurn = 0;

    if (p->altarGuardBonus > 0) {
        block += p->altarGuardBonus;
        p->altarGuardBonus = 0;
    }

    while (1) {
        if (enemy.hp <= 0) {
            int g = 5 + rand() % 8;
            p->gold += g;

            char buf[128];
            sprintf_s(buf, sizeof(buf), "승리! 경험치 획득 + 골드 %d. 아무 키.", g);
            renderMessage(buf);
            wait_any_key();

            gainExp(p, enemy.expReward);

            if ((rand() % 100) < 10) {
                renderMessage("보상 발견! 스킬 1개 해금. 아무 키.");
                wait_any_key();
                UnlockOneSkill(p, p->floor);
            }
            return 1;
        }

        if (p->HP <= 0) {
            renderMessage("패배... 체크포인트로 이동한다. 아무 키.");
            wait_any_key();
            RespawnAtCheckpoint(p);
            return 0;
        }

        DB_BeginFrame();
        drawUIFrame();
        playerstate(p);
        renderSkillBookObject();

        clearBoxInner(M_X, M_Y, M_W, M_H);
        gotoxy(M_X + 2, M_Y + 2);  printf("<< 전투 >>");
        gotoxy(M_X + 2, M_Y + 4);  printf("적: %-10s 체력:%3d/%3d", enemy.name, enemy.hp, enemy.maxHp);
        gotoxy(M_X + 2, M_Y + 8);  printf("에너지:%d/%d 방어막:%d", energy, ENERGY_MAX, block);
        gotoxy(M_X + 2, M_Y + 18); printf("조작: ↑↓ 선택 | ENTER 사용 | SPACE 턴종료 | ESC 도주(임시)");

        gotoxy(M_X + 2, M_Y + 6);
        printf("상태: ");
        if (enemy.poison > 0) printf("[독%d] ", enemy.poison);
        if (enemy.burn > 0)   printf("[화%d] ", enemy.burn);
        if (enemy.vuln > 0)   printf("[취%d] ", enemy.vuln);
        if (enemy.weak > 0)   printf("[약%d] ", enemy.weak);

        drawEquippedSkillsUI(p, selected);
        DB_Present();

        int ch = _getch();

        if (ch == 27) { // ESC 도주
            int roll = rand() % 100;
            if (roll < 50) {
                renderMessage("가까스로 전투에서 벗어났다. 아무 키.");
                wait_any_key();
                return 0;
            }
            else {
                int dmg = enemy.atk;
                renderMessage("도주 실패. 적의 반격을 받았다. 아무 키.");
                wait_any_key();

                int taken = dmg;
                if (block > 0) {
                    int used = (block >= taken) ? taken : block;
                    block -= used;
                    taken -= used;
                }
                if (taken > 0) {
                    p->HP -= taken;
                    if (p->HP < 0) p->HP = 0;
                }

                char buf[128];
                sprintf_s(buf, sizeof(buf), "반격 피해 %d! (체력 %d/%d) 아무 키.", dmg, p->HP, p->maxHP);
                renderMessage(buf);
                wait_any_key();

                ApplyTurnEnd_Player(p);
                ApplyTurnEnd_Enemy(&enemy);

                energy = ENERGY_MAX;
                zeroUsedThisTurn = 0;
                block = 0;
                continue;
            }
        }

        if (ch == ' ') { // 턴 종료
            ApplyTurnEnd_Player(p);
            enemyTurn(p, &enemy, &block);
            ApplyTurnEnd_Enemy(&enemy);

            energy = ENERGY_MAX;
            zeroUsedThisTurn = 0;
            block = 0;
            continue;
        }

        if (ch == 13) { // ENTER 사용
            if (p->equip[selected] == -1) {
                renderMessage("이 슬롯은 비어있다. 도감(E)에서 장착해라. 아무 키.");
                wait_any_key();
            }
            else {
                Skill* s = &codexSkills[p->equip[selected]];
                if (s && s->cost == 0 && zeroUsedThisTurn) {
                    renderMessage("0비용 스킬은 턴당 1회만 사용 가능! (아무 키)");
                    wait_any_key();
                    continue;
                }
                if (applySkill(p, &enemy, &block, &energy, s)) {
                    if (s && s->cost == 0) zeroUsedThisTurn = 1;
                }
                continue;
            }
        }

        if (ch == 224 || ch == 0) {
            ch = _getch();
            if (ch == 72) { // up
                selected--;
                if (selected < 0) selected = MAX_EQUIP - 1;
            }
            else if (ch == 80) { // down
                selected++;
                if (selected >= MAX_EQUIP) selected = 0;
            }
        }
    }
}

// --------------------
// 랜덤 문장
// --------------------
const char* pickRandomLine()
{
    static const char* lines[] = {
        "음습한 던전 속에서 고요함이 느껴진다...",
        ".....",
        "어둠 속에서 청각이 예민해진 기분이다..",
         ".....",
        "이 길이 맞는 것 같은 느낌이든다.",
         ".....",
        "뭔가 수상한 기운이 느껴진다.",
         ".....",
        "무언의 압박감이 느껴진다...",
         ".....",
        "발소리가 들린다.",
         ".....",
        "떨리는 심장의 파동이 온 몸을 관통한다.",
         ".....",
        "정적 속에서 심장 소리만이 또렷이 들린다."
         ".....",
    };
    int count = (int)(sizeof(lines) / sizeof(lines[0]));
    return lines[rand() % count];
}

void renderAction(int ch)
{
    gotoxy(MSG_X + 2, MSG_Y + 4);
    if (ch == 'w' || ch == 'W') printf("%-66s", "앞으로 이동했다");
    else if (ch == 's' || ch == 'S') printf("%-66s", "뒤로 이동했다");
    else if (ch == 'a' || ch == 'A') printf("%-66s", "왼쪽을 바라본다");
    else if (ch == 'd' || ch == 'D') printf("%-66s", "오른쪽을 바라본다");
    else if (ch == 'f' || ch == 'F') printf("%-66s", "층을 이동한다");
    else if (ch == 'e' || ch == 'E') printf("%-66s", "도감을 연다");
    else printf("%-66s", "...");
}

// --------------------
// 이동/던전
// --------------------
void turnL(Player* p) { p->dir = (Dir)((p->dir + 3) % 4); }
void turnR(Player* p) { p->dir = (Dir)((p->dir + 1) % 4); }

void wfront(Dir d, int* fx, int* fy)
{
    *fx = 0; *fy = 0;
    if (d == NORTH) *fy = -1;
    else if (d == SOUTH) *fy = 1;
    else if (d == WEST) *fx = -1;
    else if (d == EAST) *fx = 1;
}

int movefront(Player* p, char map[H][W + 1])
{
    int fx, fy;
    wfront(p->dir, &fx, &fy);
    int nx = p->x + fx;
    int ny = p->y + fy;

    if (nx >= 0 && nx < W && ny >= 0 && ny < H && map[ny][nx] != '#') {
        p->x = nx;
        p->y = ny;
        return 1;
    }
    return 0;
}

int moveBack(Player* p, char map[H][W + 1])
{
    int fx, fy;
    wfront(p->dir, &fx, &fy);
    int nx = p->x - fx;
    int ny = p->y - fy;

    if (nx >= 0 && nx < W && ny >= 0 && ny < H && map[ny][nx] != '#') {
        p->x = nx;
        p->y = ny;
        return 1;
    }
    return 0;
}

int checkEncounterAfterSafeMoves(void)
{
    moveCountSinceLastBattle++;
    if (moveCountSinceLastBattle <= 6) return 0;
    return (rand() % 100) < 35;
}

void renderMaze(Player* p, char map[H][W + 1])
{
    int ox = M_X + 2;
    int oy = M_Y + 2;

    for (int y = 0; y < H; y++) {
        gotoxy(ox, oy + y);
        for (int x = 0; x < W; x++) {
            if (p->x == x && p->y == y) {
                printf("%c ", dirchar(p->dir));
            }
            else if (map[y][x] == '#') {
                printf("##");
            }
            else {
                char t = map[y][x];
                if (t == 'B' || t == 'C' || t == 'S' || t == 'E' || t == 'U' || t == 'I' || t == '!' || t == '@')
                    printf("%c ", t);
                else
                    printf("  ");
            }
        }
    }
}

// 층별 맵
void LoadMapForFloor(int floor, char map[H][W + 1])
{
    const char* base1[H] = {
        "#################################",
        "#.#B..#U....#...................#",
        "#.#...#.......########!########.#",
        "#.###.#######.#####.............#",
        "#................C#B...........I#",
        "######.##########################",
        "#....#.#.........U#.............#",
        "#..B.#.#I.........###.#.........#",
        "#....#.########.##..#.####!######",
        "#....................C#........I#",
        "#.#####.#.............#....######",
        "#.....#.###############...C######",
        "#....S#...........#B.........@.E#",
        "#################################"
    };

    const char* base2[H] = {
        "#################################",
        "#..#..............#.............#",
        "#..#..######..###.#..#####..##..#",
        "#..#..#....#..#...#..#....#..#..#",
        "#..####....####...####....####..#",
        "#......#...............#........#",
        "######..#.#########..#.#######..#",
        "#......#.#...........#.#........#",
        "#.######.#..########.#.#.#####..#",
        "#.#......#..#........#.#.#......#",
        "#.#.########.#.#######.#.#.###..#",
        "#.#............#...........#....#",
        "#..............#................#",
        "#################################"
    };

    const char* base3[H] = {
        "#################################",
        "#..#..............#.............#",
        "#..#..######..###.#..#####..##..#",
        "#..#..#....#..#...#..#....#..#..#",
        "#..####....####...####....####..#",
        "#......#...............#........#",
        "######..#.#########..#.#######..#",
        "#......#.#...........#.#........#",
        "#.######.#..########.#.#.#####..#",
        "#.#......#..#........#.#.#......#",
        "#.#.########.#.#######.#.#.###..#",
        "#.#............#...........#....#",
        "#..............#................#",
        "#################################"
    };

    const char** src = base1;
    if (floor == 1) src = base2;
    else if (floor == 2) src = base3;

    for (int y = 0; y < H; y++) {
        strcpy_s(map[y], W + 1, src[y]);
    }
}

void HandleTileEvent(Player* p, char map[H][W + 1])
{
    char t = map[p->y][p->x];

    if (t == 'C') {
        p->hasCheckpoint = 1;
        p->cpX = p->x;
        p->cpY = p->y;
        renderMessage("체크포인트 등록! 아무 키.");
        wait_any_key();
        return;
    }

    if (t == 'B') {
        renderMessage("상자를 발견했다! 스킬 1개 해금. 아무 키.");
        wait_any_key();
        UnlockOneSkill(p, p->floor);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == 'S') {
        renderMessage("상인을 만났다! (스킬 1개 해금) 아무 키.");
        wait_any_key();
        UnlockOneSkill(p, p->floor);
        return;
    }

    if (t == 'I') {
        int j = GetJournalIndex(p->floor, p->x, p->y);
        if (j != -1) {
            HandleJournalEvent(p, p->floor, j);
            wait_any_key();
            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            DB_Present();
            map[p->y][p->x] = '.';
        }
        else {
            renderMessage("이 일지는 좌표표에 등록이 안 됨!");
            wait_any_key();
        }
        return;
    }

    if (t == 'U') {
        renderMessage("제단 발견! 등가교환이 가능하다. 아무 키.");
        wait_any_key();
        AltarEvent(p);
        system("cls");
        drawUIFrame();
        playerstate(p);
        renderSkillBookObject();
        renderMaze(p, map);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == '!') {
        renderMessage("강적의 기척! 강적 전투 시작. 아무 키.");
        wait_any_key();
        battleLoop(p);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == '@') {
        renderMessage("보스급 강적이다! (임시로 강적 전투) 아무 키.");
        wait_any_key();
        battleLoop(p);
        map[p->y][p->x] = '.';
        return;
    }

    if (t == 'E') {
        p->floor++;
        if (p->floor >= FLOORS) p->floor = FLOORS - 1; // 안전
        LoadMapForFloor(p->floor, map);
        p->x = 1;
        p->y = 1;

        drawUIFrame();
        playerstate(p);
        renderSkillBookObject();
        renderMaze(p, map);
        renderMessage("다음 층으로 이동했다. 아무 키.");
        wait_any_key();
    }
}

void ApplyJournalVisibility(Player* p, char map[H][W + 1], int floor)
{
    for (int i = 0; i < 3; i++) {
        if (p->journalMask[floor] & (1 << i)) {
            int x = JOURNAL_POS[floor][i].x;
            int y = JOURNAL_POS[floor][i].y;
            if (x >= 0 && x < W && y >= 0 && y < H) map[y][x] = '.';
        }
    }
}

// 일지 아카이브
void OpenJournalArchive(Player* p, char map[H][W + 1])
{
    int floorPage = p->floor;
    int sel = 0;
    int maxJ = 3;
    int needRedraw = 1;
    int first = 1;

    while (1) {
        if (needRedraw) {
            if (first) {
                drawbox(15, 4, 130, 36, " 일지 아카이브 ");
                clearBoxInner(15, 4, 130, 36);
                first = 0;
            }

            clearArea(18, 6, 120, 1);
            gotoxy(18, 6);
            printf("A/D: 층 이동 | ↑↓: 선택 | ENTER: 보기 | ESC: 닫기 (층 %d, 읽음 %d/3)",
                floorPage + 1, CountReadJournals(p, floorPage));

            clearArea(18, 10, 120, 6);
            for (int i = 0; i < maxJ; i++) {
                int y = 10 + i * 2;
                int read = (p->journalMask[floorPage] & (1u << i)) ? 1 : 0;
                gotoxy(20, y);
                printf("%c %d) %s", (i == sel) ? '>' : ' ', i + 1, read ? "읽은 일지" : "???? (미발견)");
            }

            needRedraw = 0;
            DB_Present();
        }

        int ch = _getch();

        if (ch == 27) {
            ClearPendingInputSafe();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);
            return;
        }

        if (ch == 'a' || ch == 'A') {
            if (floorPage > 0) floorPage--;
            sel = 0;
            needRedraw = 1;
            continue;
        }

        if (ch == 'd' || ch == 'D') {
            if (floorPage < FLOORS - 1) floorPage++;
            sel = 0;
            needRedraw = 1;
            continue;
        }

        if (ch == 13) {
            int read = (p->journalMask[floorPage] & (1u << sel)) ? 1 : 0;
            if (!read) {
                renderMessage("아직 발견하지 못한 일지다. 아무 키.");
                wait_any_key();
                continue;
            }

            DrawJournal(floorPage, sel);

            ClearPendingInputSafe();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            first = 1;
            needRedraw = 1;
            continue;
        }

        if (ch == 224 || ch == 0) {
            int k = _getch();
            int prev = sel;

            if (k == 72) {
                sel--;
                if (sel < 0) sel = maxJ - 1;
            }
            else if (k == 80) {
                sel++;
                if (sel >= maxJ) sel = 0;
            }

            if (sel != prev) needRedraw = 1;
        }
    }
}

// 던전 루프
void dungeonLoop(Player* p)
{
    char map[H][W + 1];
    LoadMapForFloor(p->floor, map);

    if (p->x == 0 && p->y == 0) {
        p->x = 1;
        p->y = 1;
    }

    DB_BeginFrame();
    drawUIFrame();
    playerstate(p);
    renderSkillBookObject();
    renderMaze(p, map);
    renderMessage(pickRandomLine());
    renderAction(0);

    gotoxy(42, 22);
    printf("이동: W S | 방향전환: A D | 스킬 북: E | 일지: J | 목숨을 끊는다: Q");

    DB_Present();

    while (1) {
        int moved = 0;
        int ch = _getch();

        if (ch == 'Q' || ch == 'q') break;

        if (ch == 'w' || ch == 'W') moved = movefront(p, map);
        else if (ch == 's' || ch == 'S') moved = moveBack(p, map);
        else if (ch == 'a' || ch == 'A') turnL(p);
        else if (ch == 'd' || ch == 'D') turnR(p);

        if (ch == 'e' || ch == 'E') {
            OpenCodexScene(p);
            ClearPendingInputSafe();

            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            gotoxy(42, 22);
            printf("이동: W S | 방향전환: A D | 스킬 북: E | 일지: J | 목숨을 끊는다: Q");

            DB_Present();
            continue;
        }

        if (ch == 'j' || ch == 'J') {
            OpenJournalArchive(p, map);
            ClearPendingInputSafe();

            DB_BeginFrame();
            drawUIFrame();
            playerstate(p);
            renderSkillBookObject();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);

            gotoxy(42, 22);
            printf("이동: W S | 방향전환: A D | 스킬 북: E | 일지: J | 목숨을 끊는다: Q");

            DB_Present();
            continue;
        }

        renderMaze(p, map);
        renderMessage(pickRandomLine());
        renderAction(ch);
        playerstate(p);
        renderSkillBookObject();

        if (moved) {
            HandleTileEvent(p, map);

            if (checkEncounterAfterSafeMoves()) {
                renderMessage("적의 기척이 느껴진다... 아무 키.");
                wait_any_key();

                battleLoop(p);
                moveCountSinceLastBattle = 0;

                DB_BeginFrame();
                drawUIFrame();
                playerstate(p);
                renderSkillBookObject();
                renderMaze(p, map);
                renderMessage(pickRandomLine());
                renderAction(0);

                gotoxy(42, 22);
                printf("이동: W S | 방향전환: A D | 스킬 북: E | 일지: J | 목숨을 끊는다: Q");


            }
        }
        DB_Present();
    }
}

// --------------------
// 메인
// --------------------
int main()
{
    Player player;
    memset(&player, 0, sizeof(player));

    setConsoleSize(160, 45);
    showCursor(0);

    DB_Init();

    // showStartScreen();
    // showNarration(&player);

    srand((unsigned)time(NULL));
    InitCodex();

    strcpy_s(player.name, sizeof(player.name), "방랑자");
    player.gold = 18;

    player.maxHP = 100;
    player.HP = player.maxHP;

    player.x = 1;
    player.y = 1;

    player.dir = SOUTH;

    player.level = 1;
    player.exp = 0;
    player.expToNext = 10;

    player.floor = 0;
    player.hasCheckpoint = 0;
    player.cpX = 1;
    player.cpY = 1;

    for (int i = 0; i < MAX_EQUIP; i++) player.equip[i] = -1;
    player.equipCount = 0;

    // 시작 장착: 기본 3개(공격/방어/회복)
    player.equip[0] = 0;
    player.equip[1] = 1;
    player.equip[2] = 2;
    RecountEquip(&player);

    drawUIFrame();
    playerstate(&player);
    renderSkillBookObject();

    dungeonLoop(&player);
    return 0;
}
