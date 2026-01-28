#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#define CARD_UI_X (UI_W-26)
#define CARD_UI_Y 4

#define TAG_ATTACK   (1 << 0)
#define TAG_SETUP    (1 << 1)
#define TAG_TRIGGER  (1 << 2)

#define CODEX_COLS 6
#define CODEX_ROWS 2

#define SLOT_W 22
#define SLOT_H 12

#define CODEX_X 6
#define CODEX_Y 4

#define SLOT_GAP_X 3
#define SLOT_GAP_Y 3

int moveCountSinceLastBattle;

typedef enum {
    NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3
} Dir;

typedef struct {
    int x, y;
    int HP;
    char name[50];
    Dir dir;

    int level;
    int exp;
    int expToNext;
} Player;

typedef struct {
    char name[20];
    int hp;
    int maxHp;
    int atk;
    int intent;   // 이번 턴 행동(0=공격,1=강공,2=수비 등)
    int expReward;
} Enemy;

Enemy makeEnemy()
{
    Enemy e;
    strcpy(e.name, "Slime");
    e.maxHp = 40;
    e.hp = e.maxHp;
    e.atk = 8;
    e.intent = 0;
    e.expReward = 7;
    return e;
}

// 카드 구조체 추가
typedef struct {
    int id;
    char name[20];
    int cost;
    int dmg, block, heal;
    int unlocked;        // 0 = 잠김, 1 = 해금
    char dec[60];        // 설명
} Card;

#define MAX_CARDS 16

Card codexCards[MAX_CARDS];
int codexCount = 0;

char dirchar(Dir d) {
    switch (d) {
    case NORTH: return '^';
    case EAST:  return '>';
    case SOUTH: return 'v';
    case WEST:  return '<';
    }
    return '?';
}

void gotoxy(int x, int y) {
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

void showCursor(int show) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &info);
}

void setConsoleSize(int width, int height) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    COORD bufferSize = { (SHORT)width, (SHORT)height };
    SetConsoleScreenBufferSize(h, bufferSize);

    SMALL_RECT windowSize = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
    SetConsoleWindowInfo(h, TRUE, &windowSize);
}

void waitenter() {
    int eh;
    while (1) {
        eh = _getch();
        if (eh == 13) break;
    }
}

void showStartScreen() {
    system("cls");

    printf("\n\n");
    for (int i = 0; i < 120; i++) printf("=");
    printf("\n\n");

    printf("                                                  DUNGEON GAME\n\n\n");
    for (int i = 0; i < 120; i++) printf("=");
    printf("\n\n");

    printf("                                            press enter key to start\n");
    waitenter();
}

void showNarration(Player* p) {
    system("cls");
    printf("\n");
    printf("====================================\n");
    printf("              NARRATION             \n");
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
    printf("              NARRATION             \n");
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
    printf("              NARRATION             \n");
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
    printf("              NARRATION             \n");
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

void sw() {
    gotoxy(6, 33);
    printf("        ");
    for (int i = 0; i <= 130; i++) {
        printf("-");
    }

    gotoxy(6, 34); printf("       /");
    gotoxy(6, 35); printf("      /");
    gotoxy(6, 36); printf("     / ");
    gotoxy(6, 37); printf("    /");
    gotoxy(6, 38); printf("   /");
    gotoxy(6, 39); printf("  /");
    gotoxy(6, 40); printf(" /");
    gotoxy(6, 41); printf("/");
    gotoxy(5, 42);
    for (int i = 0; i <= 134; i++) {
        printf("-");
    }
}

void drawbox(int x, int y, int w, int h, const char* title) {
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

void clearBoxInner(int x, int y, int w, int h) {
    for (int r = 1; r < h - 1; r++) {
        gotoxy(x + 1, y + r);
        for (int i = 0; i < w - 2; i++) putchar(' ');
    }
}

void drawUIFrame() {
    system("cls");
    drawbox(0, 0, UI_W, UI_H, "");
    drawbox(M_X, M_Y, M_W, M_H, "");
    drawbox(MSG_X, MSG_Y, MSG_W, MSG_H, " MESSAGE ");
    drawbox(STAT_X, STAT_Y, STAT_W, STAT_H, " STATUS ");
    drawbox(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H, " DECK ");
}
void ShowCardDetail(Card c)
{
    system("cls");
    drawbox(20, 5, 120, 30, " CARD DETAIL ");

    if (!c.unlocked) {
        gotoxy(24, 10); printf("LOCKED CARD");
        gotoxy(24, 12); printf("unlock needed!");
        gotoxy(24, 28); printf("ENTER to return");
        while (_getch() != 13);
        return;
    }

    gotoxy(24, 9);  printf("NAME : %s", c.name);
    gotoxy(24, 11); printf("COST : %d", c.cost);
    gotoxy(24, 13); printf("DMG  : %d", c.dmg);
    gotoxy(24, 14); printf("BLOCK: %d", c.block);
    gotoxy(24, 15); printf("HEAL : %d", c.heal);
    gotoxy(24, 18); printf("DESC : %s", c.dec);

    gotoxy(24, 28); printf("ENTER to return");
    while (_getch() != 13);
}
void DrawCodexSlot(int x, int y, int w, int h, const Card* c, int hasCard, int selected)
{
    // 테두리
    gotoxy(x, y); putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    for (int r = 1; r < h - 1; r++) {
        gotoxy(x, y + r); putchar('|');
        gotoxy(x + w - 1, y + r); putchar('|');
    }

    gotoxy(x, y + h - 1); putchar('+');
    for (int i = 0; i < w - 2; i++) putchar('-');
    putchar('+');

    // 선택 표시(좌우 화살표 느낌)
    if (selected) {
        gotoxy(x - 2, y + h / 2); printf(">");
        gotoxy(x + w + 1, y + h / 2); printf("<");
    }

    // 내용
    if (!hasCard) {
        gotoxy(x + 2, y + 2);
        printf("(EMPTY)");
        return;
    }

    if (c->unlocked) {
        gotoxy(x + 2, y + 2);  printf("%-18s", c->name);
        gotoxy(x + 2, y + 4);  printf("COST: %d", c->cost);
        gotoxy(x + 2, y + 6);  printf("D:%d B:%d", c->dmg, c->block);
        gotoxy(x + 2, y + 7);  printf("H:%d", c->heal);
        gotoxy(x + 2, y + 9);  printf("%-18.18s", c->dec);
    }
    else {
        gotoxy(x + 2, y + 2);  printf("???? (LOCKED)");
        gotoxy(x + 2, y + 5);  printf("unlock!");
    }
}
void OpenCodexScene(Card* cards, int count)
{
    int slotsPerPage = CODEX_COLS * CODEX_ROWS;
    int page = 0;
    int selected = 0;

    while (1) {
        system("cls");
        drawbox(0, 0, UI_W, UI_H, "");
        drawbox(2, 1, UI_W - 4, UI_H - 2, " CODEX ");

        // 페이지 안내
        gotoxy(6, 2);
        printf("E/ESC: 닫기 | ENTER: 상세 | 방향키: 이동 | A/D: 페이지  (page %d)", page + 1);

        int startIndex = page * slotsPerPage;

        // 그리드 출력
        for (int r = 0; r < CODEX_ROWS; r++) {
            for (int c = 0; c < CODEX_COLS; c++) {
                int slotIdx = r * CODEX_COLS + c;
                int cardIdx = startIndex + slotIdx;

                int x = CODEX_X + c * (SLOT_W + SLOT_GAP_X);
                int y = CODEX_Y + r * (SLOT_H + SLOT_GAP_Y);

                int hasCard = (cardIdx < count);
                int isSel = (slotIdx == selected);

                if (hasCard) DrawCodexSlot(x, y, SLOT_W, SLOT_H, &cards[cardIdx], 1, isSel);
                else        DrawCodexSlot(x, y, SLOT_W, SLOT_H, NULL, 0, isSel);
            }
        }

        int ch = _getch();

        // 닫기
        if (ch == 'e' || ch == 'E' || ch == 27) break;

        // 페이지 이동
        if (ch == 'a' || ch == 'A') {
            if (page > 0) page--;
            selected = 0;
            continue;
        }
        if (ch == 'd' || ch == 'D') {
            int maxPage = (count - 1) / slotsPerPage;
            if (page < maxPage) page++;
            selected = 0;
            continue;
        }

        // 상세보기
        if (ch == 13) {
            int idx = page * slotsPerPage + selected;
            if (idx < count) ShowCardDetail(cards[idx]);
            continue;
        }

        // 방향키 이동(그림판처럼 칸 이동)
        if (ch == 224 || ch == 0) {
            ch = _getch();
            int row = selected / CODEX_COLS;
            int col = selected % CODEX_COLS;

            if (ch == 72 && row > 0) row--;                 // ↑
            else if (ch == 80 && row < CODEX_ROWS - 1) row++; // ↓
            else if (ch == 75 && col > 0) col--;             // ←
            else if (ch == 77 && col < CODEX_COLS - 1) col++; // →

            selected = row * CODEX_COLS + col;
        }
    }
}
void InitCodex()
{
    codexCount = 0;

    codexCards[codexCount++] = (Card){ 0,"Attack",1,10,0,0,1,"기본 공격" };
    codexCards[codexCount++] = (Card){ 1,"Defend",1,0,5,0,1,"방어" };
    codexCards[codexCount++] = (Card){ 2,"Heal",2,0,0,8,1,"회복" };

    codexCards[codexCount++] = (Card){ 3,"Poison",2,12,0,0,0,"독 카드(잠김)" };
}

void playerstate(Player* p) {
    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%s : ", p->name);
    gotoxy(STAT_X + 12, STAT_Y + 2);
    printf("LEVEL %d", p->level);
    gotoxy(STAT_X + 2, STAT_Y + 4);
    printf("HP %d", p->HP);
}

void renderDeckObject(int deckCount) {
    int x = DECK_BOX_X + 3;
    int y = DECK_BOX_Y + 2;

    gotoxy(DECK_BOX_X + 2, DECK_BOX_Y + 1);
    printf("       COUNT: %d", deckCount);

    gotoxy(x, y + 0);  printf("    ______________");
    gotoxy(x, y + 1);  printf("   /___________/||");
    gotoxy(x, y + 2);  printf("  +------------+||");
    gotoxy(x, y + 3);  printf("  |            |||");
    gotoxy(x, y + 4);  printf("  |  CARD DECK |||");
    gotoxy(x, y + 5);  printf("  |            |||");
    gotoxy(x, y + 6);  printf("  |            |||");
    gotoxy(x, y + 7);  printf("  |            |||");
    gotoxy(x, y + 8);  printf("  |            |||");
    gotoxy(x, y + 9);  printf("  |            |||");
    gotoxy(x, y + 10); printf("  +------------+/");
}

void turnL(Player* p) { p->dir = (Dir)((p->dir + 3) % 4); }
void turnR(Player* p) { p->dir = (Dir)((p->dir + 1) % 4); }

void wfront(Dir d, int* fx, int* fy) {
    *fx = 0; *fy = 0;
    if (d == NORTH) *fy = -1;
    else if (d == SOUTH) *fy = 1;
    else if (d == WEST)  *fx = -1;
    else if (d == EAST)  *fx = 1;
}

int movefront(Player* p, char map[H][W + 1]) {
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

int moveBack(Player* p, char map[H][W + 1]) {
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

int checkEncounterAfter4Safe30(void) {
    moveCountSinceLastBattle++;

    if (moveCountSinceLastBattle <= 6)
        return 0;

    return (rand() % 100) < 35;
}

void renderMaze(Player* p, char map[H][W + 1]) {
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
                printf("  ");
            }
        }
    }
}

void renderMessage(const char* msg) {
    gotoxy(MSG_X + 2, MSG_Y + 2);
    printf("%-66s", msg ? msg : "...");
}

// 카드 UI + 배틀 루프
void drawboxcard(int x, int y, const Card* c, int selected) {
    gotoxy(x, y);     printf("+------------+");
    gotoxy(x, y + 1); printf("|            |");
    gotoxy(x, y + 2); printf("|            |");
    gotoxy(x, y + 3); printf("|            |");
    gotoxy(x, y + 4); printf("|            |");
    gotoxy(x, y + 5); printf("|            |");
    gotoxy(x, y + 6); printf("|            |");
    gotoxy(x, y + 7); printf("+------------+");

    if (selected) {
        gotoxy(x - 2, y + 3);  printf(">>");
        gotoxy(x + 14, y + 3); printf("<<");
    }

    gotoxy(x + 2, y + 1);  printf("%-8s", c->name);
    gotoxy(x + 10, y + 1); printf("%2d", c->cost);
    gotoxy(x + 2, y + 4);  printf("D:%2d B:%2d", c->dmg, c->block);
    gotoxy(x + 2, y + 5);  printf("H:%2d", c->heal);
    gotoxy(x + 2, y + 2);  printf("%-10.10s", c->dec);
}

void drawVerticalCards(int basex, int basey, Card hand[], int n, int selected) {
    int gapY = 9;
    for (int i = 0; i < n; i++) {
        drawboxcard(basex, basey + i * gapY, &hand[i], (i == selected));
    }
}

int initHand(Card hand[], int max) {
    int n = 0;
    if (max < 3) return 0;

    strcpy(hand[n].name, "Attack");
    hand[n].cost = 1;
    hand[n].dmg = 10;
    hand[n].block = 0;
    hand[n].heal = 0;
    strcpy(hand[n].dec, "Basic ATK");
    n++;

    strcpy(hand[n].name, "Defend");
    hand[n].cost = 1;
    hand[n].dmg = 0;
    hand[n].block = 5;
    hand[n].heal = 0;
    strcpy(hand[n].dec, "Block DMG");
    n++;

    strcpy(hand[n].name, "Heal");
    hand[n].cost = 2;
    hand[n].dmg = 0;
    hand[n].block = 0;
    hand[n].heal = 8;
    strcpy(hand[n].dec, "Restore HP");
    n++;

    return n;
}

int updateSelectedCard(int selected, int cardCount, int ch) {
    if (ch == 224 || ch == 0) { // 방향키 prefix
        ch = _getch();
        if (ch == 72) { // ↑
            selected--;
            if (selected < 0) selected = cardCount - 1;
        }
        else if (ch == 80) { // ↓
            selected++;
            if (selected >= cardCount) selected = 0;
        }
    }
    return selected;
}

void useSelectedCard(Player* p, const Card* c) {
    char buf[128];

    // 일단 메시지 출력
    sprintf(buf, "Used: %s (DMG:%d BLOCK:%d HEAL:%d)", c->name, c->dmg, c->block, c->heal);
    renderMessage(buf);

    // 실제 반영은 예시로 heal만
    if (c->heal > 0) {
        p->HP += c->heal;
        if (p->HP > 999) p->HP = 999; // 임시 상한
    }

    playerstate(p);
    Sleep(400);
}

void gainExp(Player* p, int amount)
{
    p->exp += amount;

    while (p->exp >= p->expToNext) {
        p->exp -= p->expToNext;
        p->level++;

        // 성장 규칙(너가 원하는대로 나중에 바꿔도 됨)
        p->HP += 10;
        p->expToNext += 5;

        renderMessage("LEVEL UP! HP +10");
        Sleep(600);
    }
}

int applyCard(Player* p, Enemy* e, int* block, int* energy, const Card* c)
{
    char buf[128];

    if (*energy < c->cost) {
        renderMessage("에너지가 부족하다!");
        Sleep(250);
        return 0;
    }

    *energy -= c->cost;

    if (c->dmg > 0) {
        e->hp -= c->dmg;
        if (e->hp < 0) e->hp = 0;
    }
    if (c->block > 0) {
        *block += c->block;
    }
    if (c->heal > 0) {
        p->HP += c->heal;
        if (p->HP > 999) p->HP = 999;
    }

    sprintf(buf, "Used %s | EN:%d | EnemyHP:%d", c->name, *energy, e->hp);
    renderMessage(buf);
    Sleep(250);
    return 1;
}

void enemyTurn(Player* p, Enemy* e, int* block)
{
    char buf[128];

    // 이번 턴 공격 결정(간단)
    int dmg = e->atk;
    e->intent = rand() % 2;      // 0=공격,1=강공
    if (e->intent == 1) dmg += 4;

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

    sprintf(buf, "%s attacks! DMG:%d", e->name, dmg);
    renderMessage(buf);
    Sleep(450);

    // 턴 종료 시 방어도 사라짐(슬더스 스타일)
    *block = 0;
}

void battleLoop(Player* p)
{
    Card hand[5];
    int handCount = initHand(hand, 5);
    int selectedCard = 0;

    Enemy enemy = makeEnemy();

    int energyMax = 3;
    int energy = energyMax;
    int block = 0;

    while (1) {
        // 승패 체크
        if (enemy.hp <= 0) {
            renderMessage("승리! 경험치를 획득했다.");
            Sleep(700);
            gainExp(p, enemy.expReward);
            break;
        }
        if (p->HP <= 0) {
            renderMessage("패배... (임시)"); // 나중에 게임오버 처리
            Sleep(900);
            break;
        }

        // 화면 그리기
        drawUIFrame();
        sw();
        playerstate(p);
        renderDeckObject(10);

        clearBoxInner(M_X, M_Y, M_W, M_H);
        gotoxy(M_X + 2, M_Y + 2);
        printf("<< BATTLE MODE >>");

        gotoxy(M_X + 2, M_Y + 4);
        printf("Enemy: %-10s  HP:%3d/%3d", enemy.name, enemy.hp, enemy.maxHp);

        gotoxy(M_X + 2, M_Y + 6);
        printf("ENERGY: %d/%d   BLOCK: %d", energy, energyMax, block);

        gotoxy(M_X + 2, M_Y + 8);
        printf("↑↓ 선택 | ENTER 사용 | SPACE 턴종료 | ESC 도주(임시)");

        // 카드 UI
        drawVerticalCards(CARD_UI_X, CARD_UI_Y, hand, handCount, selectedCard);

        int ch = _getch();

        if (ch == 27) { // ESC
            renderMessage("도주했다...(임시)");
            Sleep(500);
            break;
        }

        if (ch == ' ') { // 턴 종료 → 적 턴 → 내 턴 에너지 회복
            enemyTurn(p, &enemy, &block);
            energy = energyMax;
            continue;
        }

        if (ch == 13) { // 카드 사용
            applyCard(p, &enemy, &block, &energy, &hand[selectedCard]);
            continue;
        }

        selectedCard = updateSelectedCard(selectedCard, handCount, ch);
    }
}


// 던전 메시지/액션/전투 진입


void renderAction(int ch) {
    gotoxy(MSG_X + 2, MSG_Y + 3);

    if (ch == 'w' || ch == 'W')      printf("%-66s", "앞으로 이동했다");
    else if (ch == 's' || ch == 'S') printf("%-66s", "뒤로 이동했다");
    else if (ch == 'a' || ch == 'A') printf("%-66s", "왼쪽을 바라본다");
    else if (ch == 'd' || ch == 'D') printf("%-66s", "오른쪽을 바라본다");
    else   printf("%-66s", "...");
}

const char* pickRandomLine() {
    static const char* lines[] = {
        "음습한 던전 속에서 고요함이 느껴진다...",
        "어둠 속에서 청각이 예민해진 기분이다..",
        "이 길이 맞는 것 같은 느낌이든다.",
        "뭔가 수상한 기운이 느껴진다.",
        "무언의 압박감이 느껴진다...",
        "지금 처해진 상황에 대해 생각한다, 허나 이내 생각을 멈췄다.",
        "발소리가 들린다.",
        "떨리는 심장의 파동이 온 몸을 관통한다.",
        "불안감이 고조된다.",
        "검을 쥐어진 손에선 식은땀이 느껴진다.",
        "공기가 무겁게 가라앉아 폐 속으로 스며든다.",
        "정적 속에서 심장 소리만이 또렷이 들린다.",
        "벽 너머에서 무언가 움직인 듯한 착각이 든다.",
        "발걸음을 옮길 때마다 바닥이 낮게 울린다.",
        "등 뒤의 어둠이 점점 가까워지는 느낌이다.",
        "시선이 닿지 않는 곳에 무언가 있을 것만 같다.",
        "이곳에 오래 머물러선 안 될 것 같다.",
        "숨을 고르며 긴장을 억지로 눌러본다.",
        "차가운 공기가 피부를 스친다.",
        "조용함이 오히려 불안을 키운다.",
        "작은 소리에도 몸이 반사적으로 굳는다.",
        "어둠 속에서 방향 감각이 흐려진다.",
        "지금 내 선택이 옳은지 확신할 수 없다.",
        "발밑의 그림자가 낯설게 느껴진다.",
        "여기서는 시간이 느리게 흐르는 듯하다.",
        "무심코 검자루를 더 강하게 움켜쥔다.",
        "이 던전은 나를 시험하는 것 같다.",
        "어둠이 사고마저 잠식하려 든다.",
        "이곳에 남겨진 흔적들이 의미심장하다.",
        "긴장 탓인지 숨이 조금 가빠진다.",
    };

    int count = (int)(sizeof(lines) / sizeof(lines[0]));
    return lines[rand() % count];
}

void dungeonLoop(Player* p) {
    char map[H][W + 1] = {
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

    if (p->x == 0 && p->y == 0) { p->x = 1; p->y = 1; }

    renderMaze(p, map);
    renderMessage(pickRandomLine());
    renderAction(0);

    while (1) {
        int moved = 0;
        int ch = _getch();

        if (ch == 'Q' || ch == 'q') break;

        if (ch == 'w' || ch == 'W') moved = movefront(p, map);
        else if (ch == 's' || ch == 'S') moved = moveBack(p, map);
        else if (ch == 'a' || ch == 'A') turnL(p);
        else if (ch == 'd' || ch == 'D') turnR(p);
        if (ch == 'e' || ch == 'E') {
            OpenCodexScene(codexCards, codexCount);

            //던전 화면 복구
            drawUIFrame();
            playerstate(p);
            renderDeckObject(10);
            sw();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);
            continue;
        }
        renderMaze(p, map);
        renderMessage(pickRandomLine());
        renderAction(ch);
        playerstate(p);
        sw();
        renderDeckObject(10);

        if (moved && checkEncounterAfter4Safe30()) {
            renderMessage("적의 기척이 느껴진다...");
            Sleep(1100);

            // 여기서 전투 진입
            battleLoop(p);

            moveCountSinceLastBattle = 0; // 전투 후 안전 초기화

            // 던전 UI 복귀 화면 재구성
            drawUIFrame();
            playerstate(p);
            renderDeckObject(10);
            sw();
            renderMaze(p, map);
            renderMessage(pickRandomLine());
            renderAction(0);


        }
    }
}

int main() {
    // 해야할것
    // 적 디자인(강적은 나중에)
    // 랜덤 인카운터(적) / 완성
    // 고정 인카운터(상인),(상자)
    // 전투 (미완)
    // 카드 디자인
    // 카드 원소 상호작용
    // 전투시 깜빡임 마지막 더블버퍼링,,

    Player player = { 0 };

    setConsoleSize(160, 45);
    showCursor(0);

    // showStartScreen();
    strcpy_s(player.name, sizeof(player.name), "방랑자");
    // showNarration(&player);

    srand((unsigned)time(NULL));
    InitCodex();
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

    player.HP = 100;
    player.x = 1;
    player.y = 1;
    player.dir = SOUTH;
    player.level = 1;
    player.exp = 0;
    player.expToNext = 10;

    drawUIFrame();
    playerstate(&player);
    renderDeckObject(10);
    sw();
    dungeonLoop(&player);

    return 0;
}
