#include <stdio.h>
#include <conio.h>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define H 14
#define W 33

#define UI_W 130
#define UI_H 35

#define M_X 32
#define M_Y 4
#define M_W 70
#define M_H 18

#define MSG_X 32
#define MSG_Y (M_Y + M_H + 2)
#define MSG_W 70
#define MSG_H 6

#define STAT_X 3
#define STAT_Y 3
#define STAT_W 25
#define STAT_H 8

#define DECK_BOX_X 3
#define DECK_BOX_Y 12
#define DECK_BOX_W 25
#define DECK_BOX_H 18

#define TAG_ATTACK   (1 << 0)
#define TAG_SETUP    (1 << 1)
#define TAG_TRIGGER  (1 << 2)

int moveCountSinceLastBattle;

typedef enum {
    NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3
} Dir;


typedef struct {
    int x, y;
    int HP;
    char name[50];
    
    Dir dir;
} Player;

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

void waitenter()
{
    int eh;


    while (1)
    {
        eh = _getch();
        if (eh == 13) break;
    }
}

void showStartScreen()
{
    system("cls");

    printf("\n\n");
    for (int i = 0; i < 120; i++) {
        printf("=");
    } printf("\n");
    printf("\n\n");

    printf("                                                  DUNGEON GAME\n\n\n");
    for (int i = 0; i < 120; i++) {
        printf("=");
    } printf("\n\n");

    printf("                                            press enter key to start\n");

    waitenter();
}

void showNarration(Player* p)
{


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
    drawbox(M_X, M_Y, M_W, M_H, " MAP ");
    drawbox(MSG_X, MSG_Y, MSG_W, MSG_H, " MESSAGE ");
    drawbox(STAT_X, STAT_Y, STAT_W, STAT_H, " STATUS ");
    drawbox(DECK_BOX_X, DECK_BOX_Y, DECK_BOX_W, DECK_BOX_H, " DECK ");
}

void playerstate(Player*p)
{
   

    gotoxy(STAT_X + 2, STAT_Y + 2);
    printf("%s",p->name);
    gotoxy(STAT_X + 2, STAT_Y + 4);
    printf("HP 100");
    gotoxy(STAT_X + 2, STAT_Y + 6);
    printf("SP 50");
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
    // 이동 성공 후에만 호출된다고 가정

    moveCountSinceLastBattle++;

    // 1~3번째 이동은 무조건 안전
    if (moveCountSinceLastBattle <= 3)
        return 0;

    // 4번째 이동부터 30%
    return (rand() % 100) < 30;
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

void clearMazeArea(void) {
    clearBoxInner(M_X, M_Y, M_W, M_H);
}

void renderMessage(const char* msg) {
    gotoxy(MSG_X + 2, MSG_Y + 2);
    printf("%-66s", msg ? msg : "...");
}

void battlesystem()
{
    renderMessage("전투(임시): 아무 키나 누르면 종료");
    _getch();
}

void renderAction(int ch) {
    gotoxy(MSG_X + 2, MSG_Y + 3);

    if (ch == 'w' || ch == 'W')      printf("%-66s", "앞으로 이동했다");
    else if (ch == 's' || ch == 'S') printf("%-66s", "뒤로 이동했다");
    else if (ch == 'a' || ch == 'A') printf("%-66s", "왼쪽을 바라본다");
    else if (ch == 'd' || ch == 'D') printf("%-66s", "오른쪽을 바라본다");
    else                              printf("%-66s", "...");
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
       
        renderMaze(p, map);
        renderMessage(pickRandomLine());
        renderAction(ch);
        playerstate(p);
        renderDeckObject(10);
       

        if (moved && checkEncounterAfter4Safe30()) {
            renderMessage("적의 기척이 느껴진다...");
            Sleep(400);

            battlesystem(); // 전투 진입 아직 임시
            moveCountSinceLastBattle = 0; // 전투 후 안전 초기화
          
            drawUIFrame();
            playerstate(p);
            renderDeckObject(10);
            renderMaze(p, map);
         
           
        }
    }
}

int main() 
{

    Player player = { 0 };
   
    setConsoleSize(131, 36);
    showCursor(0);

   // showStartScreen();
   // showNarration(&player);

    srand((unsigned)time(NULL));
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

    player.x = 1;
    player.y = 1;
    player.dir = SOUTH;

    drawUIFrame(); 
    playerstate(&player);
    renderDeckObject(10);
    dungeonLoop(&player);
 
    return 0;
}
