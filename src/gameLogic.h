#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <ncurses.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>

#ifndef GAMELOGIC_H
#define GAMELOGIC_H

typedef struct
{
    char name[6]; // Player ID, max 5 characters + null terminator
    int score;    // Player score
    int matches;  // Number of matches played
} Player;

typedef struct
{
    int id;
    int num_players;
    int num_rounds;
    int current_round;
    int num_joined_players;
    int num_current_round;
    char *playerNames[5];
    int playerScores[5];
    bool started;
    bool active;
    char *roundData[5];
} Game;

#define FIFO_FILE "/tmp/my_fifo"
#define FIFO_FILE_RESPONSE "/tmp/my_fifo_response"

#define MAX_PLAYERS 100
#define MAX_GAMES 100
#define MAX_THREADS 100


extern Game games[MAX_GAMES];
extern int game_num;

extern Player players[MAX_PLAYERS];
extern int num_players;

extern pthread_t thread_ids[MAX_THREADS];
extern int num_threads;

extern pthread_mutex_t lock;
extern pthread_cond_t cond;


void setupForThreads(void);
void endThreads(void);

void handle_server_sigterm(int sig);
void handle_server_sigint(int sig);
void *startGameServer(void *arg);
void createNewGameServer(int num_players, int num_rounds);
char *getGameInfo(int game_id);
bool registerClient(char *client_name);
char *getLeaderBoard(void);
bool joinGame(int game_id, char *playerName);
char *listWaitingGames(char *client_name);
char *listGames(char *client_name);
void makeDecision(char *game_id_string, char *decision_string, char *client_name);

void server(void);

void commandSender(const char *command);
int is_valid_id(const char *id);

void handle_client_sigint(int sig);
void handle_client_sigterm(int sig);

void showMenu(void);
void displayWaitingRooms(bool prevJoined, int prevJoinedId, bool cantjoin);
void displayGame(int game_id);
void showCurrentGames(void);
void createNewGame(void);
void showLeaderboard(void);
int compare(const void *a, const void *b);
void printLeaderboard(Player players[], int size, char *currentPlayer);
void parseMessage(char *message, char *currentPlayer);

char *readFromServer(void);
void client(const char *id);

#endif
