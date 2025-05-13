#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <unistd.h>

#define IP "35.233.235.105"

struct mosquitto *mosq = NULL;

volatile int board_updated = 1;

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg){
	if (strcmp(msg->topic, "gameboard") == 0){
		if (msg->payloadlen < 9) {
			fprintf(stderr, "Received payload too short: %d bytes\n", msg->payloadlen);
			return;
		}
		
		const char *board = (char *)msg->payload;

		printf("\nCurrent Board:\n");

		for (int i = 0; i < 9; i++) {
			printf(" %c ", board[i]);
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


	char player_choice;
	printf("Who will go first, x or o: ");
	scanf(" %c", &player_choice);
	while (player_choice != 'x' && player_choice != 'o'){
		printf("Invalid player. Who will go first, x or o: ");
		scanf(" %c", &player_choice);
	}
	int cell = 0;
	while (cell != -1) {
		while(!board_updated){
			usleep(10000);
		}
		board_updated = 0;

		printf("\nPlayer %c's turn\n", player_choice);

		printf("Enter cell number (1-9) or enter 0 to quit: ");
		scanf("%d", &cell);
		cell--;

		if ((cell < 0 || cell > 8) && cell != -1) {
			printf("Invalid cell. Must be between 1 and 9.\n");
			continue;
		}

		if (player_choice == 'x') {
			publish_move("player/x", cell);
			player_choice = 'o';
		} else {
			publish_move("player/o", cell);
			player_choice = 'x';
		}
	}

	mosquitto_loop_stop(mosq, true);
	mosquitto_disconnect(mosq);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	return 0;
}