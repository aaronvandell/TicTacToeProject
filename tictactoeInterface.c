#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <unistd.h>

#define IP "35.233.235.105"

struct mosquitto *mosq = NULL;
char current_board[10] = "---------";

volatile int board_updated = 1;

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg){
	if (strcmp(msg->topic, "gameboard") == 0){
		if (msg->payloadlen < 9) {
			fprintf(stderr, "Received payload too short: %d bytes\n", msg->payloadlen);
			return;
		}
		
		memcpy(current_board, msg->payload, 9);
		current_board[9] = '\0';

		printf("\nCurrent Board:\n");

		for (int i = 0; i < 9; i++) {
			printf(" %c ", current_board[i]);
			if ((i + 1) % 3 == 0) printf("\n");
		}
		board_updated = 1;
	} 
}

void publish_move(const char *player_topic, int cell)
{
	if (!mosq) {
		fprintf(stderr, "Mosquitto client is not initialized.\n");
		return;
	}

	char move_str[4];
	snprintf(move_str, sizeof(move_str), "%d", cell);
	int rc = mosquitto_publish(mosq, NULL, player_topic, strlen(move_str), move_str, 0, false);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Failed to publish: %s\n", mosquitto_strerror(rc));
	}
}

int main()
{
	mosquitto_lib_init();

	mosq = mosquitto_new("TicTacToe", true, NULL);
	if (!mosq){
		fprintf(stderr, "Failed to create mosquitto client");
		return 1;
	}

	mosquitto_message_callback_set(mosq, on_message);

	if (mosquitto_connect(mosq, IP, 1883, 60) != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Failed to connect to broker\n");
		return 1;
	}

	mosquitto_subscribe(mosq, NULL, "gameboard", 0);

	mosquitto_loop_start(mosq);

	mosquitto_publish(mosq, NULL, "game/reset", 10, "7", 0, false);

	int menu_choice = '5';
	while (menu_choice != 0)
	{
		printf("Main Menu:\n1: Player vs Bot\n2: Player vs Player\n3:Bot vs Bot\n");
		printf("Enter number to select mode, or enter 0 to quit: ");
		scanf("%d", &menu_choice);

		if (menu_choice == 1)
		{
			int cell = 0;
			char player = 'x';

			while (1) {
				while(!board_updated){
					usleep(10000);
				}
				board_updated = 0;

				printf("\nYour turn\n");

				printf("Enter cell number (1-9) or enter 0 to quit: ");
				scanf("%d", &cell);
				if (cell == 0) break;
				cell--;

				while (cell < 0 || cell > 8) {
					printf("Invalid cell. Must be between 1 and 9: ");
					scanf("%d", &cell);
					if (cell == 0) break;
					cell--;
				}
				if (cell == -1) break;

				while(current_board[cell] != '-')
				{
					printf("Cell already taken, enter a new cell: ");
					scanf("%d", &cell);
					if (cell == 0) break;
					cell--;
				}
				if (cell == -1) break;

				publish_move("player/x", cell);
				printf("\nBash Bot (O) is thinking...\n");
				mosquitto_publish(mosq, NULL, "bot/start", strlen(current_board), current_board, 0, false);
				while (!board_updated) {
					usleep(10000);
				}
				board_updated = 0;
			}
		} 
		else if (menu_choice == 2)
		{
			char player_choice;
			printf("Who will go first, x or o: ");
			scanf(" %c", &player_choice);
			while (player_choice != 'x' && player_choice != 'o'){
				printf("Invalid player. Who will go first, x or o: ");
				scanf(" %c", &player_choice);
			}
			int cell = 0;
			while (1) {
				while(!board_updated){
					usleep(10000);
				}
				board_updated = 0;

				printf("\nPlayer %c's turn\n", player_choice);

				printf("Enter cell number (1-9) or enter 0 to quit: ");
				scanf("%d", &cell);
				if (cell == 0) break;
				cell--;

				while (cell < 0 || cell > 8) {
					printf("Invalid cell. Must be between 1 and 9: ");
					scanf("%d", &cell);
					if (cell == 0) break;
					cell--;
				}
				if (cell == -1) break;

				while (cell != -1 && current_board[cell] != '-') {
					printf("Cell already taken. Enter a new cell (1-9): ");
					scanf("%d", &cell);
					if (cell == 0) break;
					cell--;
				}
				if (cell == -1) break;


				if (player_choice == 'x') {
					publish_move("player/x", cell);
					player_choice = 'o';
				} else {
					publish_move("player/o", cell);
					player_choice = 'x';
				}
			}
		}
		else if(menu_choice == 3)
		{
			mosquitto_publish(mosq, NULL, "board/reset", 10, "7", 0, false); // Reset game on ESP32
			usleep(500000); // Wait for reset to propagate

			char current_player = 'x';
			int moves_made = 0;

			while (1) {
				while (!board_updated) {
					usleep(10000); // wait for board update
				}
				board_updated = 0;

				// Detect game reset (all dashes)
				int empty_cells = 0;
				for (int i = 0; i < 9; i++) {
					if (current_board[i] == '-') empty_cells++;
				}
				if (empty_cells == 9 && moves_made > 0) {
					printf("\nGame Over (Board Reset Detected)\n");
					break;
				}

				if (current_player == 'x') {
					int cell = rand() % 9;
					while (current_board[cell] != '-') {
						cell = rand() % 9;
					}
					printf("\nC Bot (X) playing cell %d\n", cell + 1);
					publish_move("player/x", cell);
					moves_made++;
				} else {
					printf("\nBash Bot (O) is thinking...\n");
					mosquitto_publish(mosq, NULL, "bot/start", strlen(current_board), current_board, 0, false);
				}

				current_player = (current_player == 'x') ? 'o' : 'x';
			}
		}
		mosquitto_publish(mosq, NULL, "board/reset", 10, "7", 0, false);

		int board_cleared = 0;
		while (!board_cleared) {
			while (!board_updated) {
				usleep(10000);
			}
			board_updated = 0;

			board_cleared = 1;
			for (int i = 0; i < 9; i++) {
				if (current_board[i] != '-') {
					board_cleared = 0;
					break;
				}
			}
		}
	}

	mosquitto_loop_stop(mosq, true);
	mosquitto_disconnect(mosq);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	return 0;
}