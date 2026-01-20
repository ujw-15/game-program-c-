#include<stdio.h>
#include<windows.h>
#include<conio.h>

#define UP 72
#define LEFT 75
#define RIGHT 77
#define DOWN 80


void position(int x, int y)
{
	COORD position = { x, y };

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), position);
}

int main()
{
	int x = 0, y = 0;
	 position(x, y);

	 printf("★");
	
	char key = 0;

	while (1)
	{
		key = _getch();

		if (key == -32 || key == 0)
		{
			key = _getch();
		}

		position(x, y);
		printf(" ");

		switch (key)
		{

		case UP : y--;
			break;

		case LEFT : x -= 2;
			break;

		case RIGHT: x += 2;
			break;

		case DOWN : y++;
			break;

		default: printf(" ");
			break;
		}

		system("cls");

		position(x, y);

		printf("★");		
	}

	return 0;
}