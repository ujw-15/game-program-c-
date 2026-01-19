#include<stdio.h>
#include<stdlib.h>
#include<time.h>

void shuffle(int array[], int size)
{
	for (int i = 0; i < size; i++)
	{
		int seed = rand() % size;

		int tmp = array[seed];

		array[seed] = array[i];

		array[i] = tmp;
	}
}

int main()
{
#pragma region 의사 난수
	// 0 ~ 32767 사이의 난수 값을 반환하는 함수입니다.

	// UTC 기준으로 1970년 1월 1일 0시 0분 0초부터 경과된
	// 시간을 초(sec)로 반환하는 함수입니다.

	// srand : rand(가 사용할 초기값(seed)을 설정하는 함수

	//srand(time(NULL));
	//
	//int random = rand() % 10 + 1;
	//
	//printf("random : %d\n", random);


#pragma endregion

#pragma region  셔플 함수

//	int array[] = { 1,2,3,4,5,6,7,8,9,10 };
//	int size = sizeof(array) / sizeof(array[0]);
//
//	srand(time(NULL));
//
//	shuffle(array, size);
//	
//	for (int i = 0; i < size; i++)
//	{
//		printf("array[%d]의 값: %d\n", i, array[i]);
//	}


#pragma endregion

#pragma region UP - DOWN 게임
//
//	srand(time(NULL));
//
//	int random = rand() % 50 + 1;
//	int life = 5;
//	int num;
//
//	while (life > 0)
//	{
//		printf("LIFE : %d\n", life);
//		printf("정수를 입력하시오(1~50): ");
//		scanf_s("%d", &num);
//		printf("\n");
//
//		if (num < random)
//		{
//			life--;
//			printf("컴퓨터가 가지고 있는 값보다 작습니다.\n");
//			
//		}
//		else if (num > random)
//		{
//			life--;
//			printf("컴퓨터가 가지고 있는 값보다 큽니다.\n");
//			
//		}
//		else
//		{
//			printf("컴퓨터가 가지고 있는 값을 맞추었습니다.\n");
//			printf("Victory\n");
//			break;
//		}
//	}
//	if (life == 0) {
//		printf("Defeat\n");
//	}
//	
//	printf("답: %d", random);

#pragma endregion

	return 0;
}