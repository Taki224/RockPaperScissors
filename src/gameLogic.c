#include "gameLogic.h"

// Global variable to store every information about games.
Game games[MAX_GAMES];
int game_num = 1;

// Global variable to store every information about players.
Player players[MAX_PLAYERS];
int num_players = 0;

// Store all the thread IDs
pthread_t thread_ids[MAX_THREADS];
int num_threads = 0;

// Mutex and condition variable to synchronize threads
pthread_mutex_t lock;
pthread_cond_t cond;

// --------------------------------------------------------
// ---------------------- SERVER --------------------------
// --------------------------------------------------------

// Handle SIGINT signal (Ctrl+C)
void handle_server_sigint(int sig)
{
    printf("Quitting...\n");
    sleep(1);
    endThreads();
    exit(0);
}

// Handle SIGTERM signal (kill)
void handle_server_sigterm(int sig)
{
    printf("Quitting...\n");
    exit(1);
}

// Initialize mutex and condition variable
void setupForThreads(void)
{
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
}

// This is called as cleanup function when the program exits
void endThreads(void)
{
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
}

// This is the function that runs on a separate thread to handle a game
// All the server side game logic is implemented here (waiting for players, all rounds, point calculation, etc.)
void *startGameServer(void *arg)
{
    int game_id = *(int *)arg;
    free(arg);
    arg = NULL;
    // This part is for waiting for players. It waits until a player joins, it gets a signal and checks if the game if full.
    bool condition = false;
    do
    {
        pthread_mutex_lock(&lock);
        condition = (games[game_id].num_joined_players == games[game_id].num_players);
        if (!condition)
        {
            pthread_cond_wait(&cond, &lock);
        }
        pthread_mutex_unlock(&lock);
    } while (!condition);
    // when the game is full it changes the state of the game to started
    pthread_mutex_lock(&lock);
    games[game_id].started = true;
    pthread_mutex_unlock(&lock);

    // The first while loop is for the rounds.
    bool condition1 = false;
    bool condition2 = false;
    do
    {
        pthread_mutex_lock(&lock);
        int current_round = games[game_id].current_round - 1;
        int decisionsCount = 0;
        pthread_mutex_unlock(&lock);
        // This is the loop for one game. It waits until all players make a decision. When a player makes a decision it sends a signal and checks if all players made a decision.
        do
        {
            pthread_mutex_lock(&lock);
            decisionsCount = 0;
            for (int i = 0; i < games[game_id].num_players; i++)
            {
                if (games[game_id].roundData[current_round][i] != 'n')
                {
                    decisionsCount++;
                }
            }
            condition1 = decisionsCount < games[game_id].num_players;
            if (condition1)
            {
                // Wait for signal from the function that handles new moves.
                pthread_cond_wait(&cond, &lock);
            }
            pthread_mutex_unlock(&lock);
        } while (condition1);
        // When a round is finished it calculates the points for each player.
        pthread_mutex_lock(&lock);
        for (int i = 0; i < games[game_id].num_players; i++)
        {
            for (int j = 0; j < games[game_id].num_players; j++)
            {
                if (i != j)
                {
                    if (games[game_id].roundData[current_round][i] == 'p')
                    {
                        if (games[game_id].roundData[current_round][j] == 'r')
                        {
                            games[game_id].playerScores[i]++;
                        }
                    }
                    if (games[game_id].roundData[current_round][i] == 'r')
                    {
                        if (games[game_id].roundData[current_round][j] == 's')
                        {
                            games[game_id].playerScores[i]++;
                        }
                    }
                    if (games[game_id].roundData[current_round][i] == 's')
                    {
                        if (games[game_id].roundData[current_round][j] == 'p')
                        {
                            games[game_id].playerScores[i]++;
                        }
                    }
                }
            }
        }
        // Go to the next round
        games[game_id].current_round++;

        // check if game is finished
        condition2 = games[game_id].current_round > games[game_id].num_rounds;

        pthread_mutex_unlock(&lock);
    } while (!condition2);
    // when game ends
    pthread_mutex_lock(&lock);
    games[game_id].active = false;
    // add points to the players sum of points
    // add 1 to the players sum of matches
    for (int i = 0; i < games[game_id].num_players; i++)
    {
        for (int j = 0; j < num_players; j++)
        {
            if (strcmp(games[game_id].playerNames[i], players[j].name) == 0)
            {
                players[j].score += games[game_id].playerScores[i];
                players[j].matches++;
                break;
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

// This function is called when the server receives a command to create a new game
// It initializes the game struct and starts a new thread to handle the game
void createNewGameServer(int player_num, int num_rounds)
{
    pthread_mutex_lock(&lock);
    games[game_num].id = game_num;
    games[game_num].num_players = player_num;
    games[game_num].num_rounds = num_rounds;
    games[game_num].current_round = 1;
    games[game_num].started = false;
    games[game_num].active = true;
    games[game_num].num_joined_players = 0;
    games[game_num].num_current_round = 0;
    for (int i = 0; i < 5; i++)
    {
        games[game_num].playerNames[i] = NULL;
        // If a player made a decision it will be put at the correct index in the roundData string. If not, it will be 'n'.
        games[game_num].roundData[i] = (char *)"nnnnn";
        games[game_num].playerScores[i] = 0;
    }
    pthread_mutex_unlock(&lock);

    int *game_id_ptr = malloc(sizeof(int));
    if (game_id_ptr == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        return;
    }
    *game_id_ptr = game_num;
    // Start game on new thread
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, startGameServer, game_id_ptr))
    {
        fprintf(stderr, "Error creating thread\n");
        // Free the allocated memory if thread creation fails
        free(game_id_ptr);
        game_id_ptr = NULL;
        return;
    }
    // Store the thread ID
    thread_ids[num_threads] = thread_id;
    num_threads++;

    // Detach the thread since we won't be joining it, it runs on its own until the game ends and reads from the global variables
    pthread_detach(thread_id);

    game_num++;
}

// This function is called when the server receives a command to get the game info
// It returns a string containing all the information about the game
// Format: GI;<num_players>;<player1>,<player2>,...;<current_round>,<num_rounds>;<player1_score>,<player2_score>,...;<round1_data>,<round2_data>,...
char *getGameInfo(int game_id)
{
    pthread_mutex_lock(&lock);
    char *game_info = malloc(BUFSIZ * sizeof(char));
    if (game_info == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        return NULL;
    }
    // check if game is started
    if (!games[game_id].started)
    {
        game_info[0] = '\0';
        strcat(game_info, "notstarted");
        return game_info;
    }
    else
    {
        game_info[0] = '\0';
    }
    // Cat all the information about the game to the game_info string
    sprintf(game_info, "GI;%d;", games[game_id].num_players);

    for (int i = 0; i < games[game_id].num_players; i++)
    {
        strcat(game_info, games[game_id].playerNames[i]);
        if (i < games[game_id].num_players - 1)
        {
            strcat(game_info, ",");
        }
    }

    strcat(game_info, ";");
    sprintf(game_info, "%s%d,%d;", game_info, games[game_id].current_round, games[game_id].num_rounds);
    for (int i = 0; i < games[game_id].num_players; i++)
    {
        sprintf(game_info, "%s%d", game_info, games[game_id].playerScores[i]);
        if (i < games[game_id].num_players - 1)
        {
            strcat(game_info, ",");
        }
    }

    strcat(game_info, ";");
    for (int i = 0; i < games[game_id].current_round; i++)
    {
        strncat(game_info, games[game_id].roundData[i], games[game_id].num_players);
        if (i < games[game_id].current_round - 1)
        {
            strcat(game_info, ",");
        }
    }
    strcat(game_info, ";");

    pthread_mutex_unlock(&lock);
    return game_info;
}

// This function is called when the server receives a command to register a new client
bool registerClient(char *client_name)
{
    // check if name is already taken
    pthread_mutex_lock(&lock);
    for (int i = 0; i < num_players; i++)
    {
        if (strcmp(players[i].name, client_name) == 0)
        {
            pthread_mutex_unlock(&lock);
            return false;
        }
    }
    // If not store the new player in the players array
    strncpy(players[num_players].name, client_name, sizeof(players[num_players].name));
    players[num_players].score = 0;
    players[num_players].matches = 0;
    num_players++;
    pthread_mutex_unlock(&lock);
    return true;
}

// This function is called when the server receives a command to get the leaderboard
char *getLeaderBoard(void)
{
    char *players_string = malloc(BUFSIZ * sizeof(char));
    if (players_string == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        return NULL;
    }
    players_string[0] = '\0';
    pthread_mutex_lock(&lock);
    // Put the information about player points and matches in the players_string
    for (int i = 0; i < num_players; i++)
    {
        char player_string[BUFSIZ];
        snprintf(player_string, sizeof(player_string), "%s,%d,%d;", players[i].name, players[i].score, players[i].matches);
        strcat(players_string, player_string);
    }
    pthread_mutex_unlock(&lock);
    return players_string;
}

// This function is called when the server receives a command to join a game
bool joinGame(int game_id, char *client_name)
{
    pthread_mutex_lock(&lock);
    // check if game is started. If it already started no player can join
    if (games[game_id].started)
    {
        pthread_mutex_unlock(&lock);
        return false;
    }
    else
    {
        games[game_id].playerNames[games[game_id].num_joined_players] = strdup(client_name);
        games[game_id].num_joined_players++;
        // If a player joined send a signal to the thread that handles the game
        // All threads get the signal but it only affects the thread that handles the game since it checks if the game is full
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
        return true;
    }
}

// This function is called when the server receives a command to list the waiting games
char *listWaitingGames(char *client_name)
{
    char *games_string = malloc(BUFSIZ * sizeof(char));
    if (games_string == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        return NULL;
    }
    games_string[0] = '\0';
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if (games[i].active && !games[i].started)
        {
            // check if client is not joined yet
            // The client can only see the games that he is not joined yet and it is not started yet
            bool joined = false;
            // The already joined players are stored in the playerNames array
            for (int j = 0; j < games[i].num_joined_players; j++)
            {
                if (strcmp(games[i].playerNames[j], client_name) == 0)
                {
                    joined = true;
                    break;
                }
            }
            // Cat all the games that the client can join to the games_string
            if (!joined)
            {
                char buffer[3];
                sprintf(buffer, "%d", i);
                strcat(games_string, buffer);
                strcat(games_string, ",");
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return games_string;
}

// This function is called when the server receives a command to list the games
// It is the opposite of the listWaitingGames function. It only shows the games that the client is already joined
char *listGames(char *client_name)
{
    char *games_string = malloc(BUFSIZ * sizeof(char));
    if (games_string == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        return NULL;
    }
    games_string[0] = '\0';
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if (games[i].active)
        {
            // check if client is joined
            bool joined = false;
            // The already joined players are stored in the playerNames array
            for (int j = 0; j < games[i].num_joined_players; j++)
            {
                if (strcmp(games[i].playerNames[j], client_name) == 0)
                {
                    joined = true;
                    break;
                }
            }
            // Cat all the games that the client is joined to the games_string
            if (joined)
            {
                char buffer[3];
                sprintf(buffer, "%d", i);
                strcat(games_string, buffer);
                strcat(games_string, ",");
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return games_string;
}

// This function is called when the server receives a command to make a decision
void makeDecision(char *game_id_string, char *decision_string, char *client_name)
{
    if (game_id_string == NULL || decision_string == NULL)
    {
        printf("Error: Invalid command format\n");
    }
    int game_id = atoi(game_id_string);
    pthread_mutex_lock(&lock);
    int current_round = games[game_id].current_round - 1;
    int player_index = -1;
    // Find the index of the player in the playerNames array
    for (int i = 0; i < games[game_id].num_players; i++)
    {
        if (strcmp(games[game_id].playerNames[i], client_name) == 0)
        {
            player_index = i;
            break;
        }
    }
    // Modify the roundData string at the correct index
    char *playerDecisionsCopy = strdup(games[game_id].roundData[current_round]);
    if (playerDecisionsCopy == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    playerDecisionsCopy[player_index] = decision_string[0];
    games[game_id].roundData[current_round] = playerDecisionsCopy;
    // Send a signal to the thread that handles the game
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
}

// This function initializes the server
void server(void)
{
    // Create all necessary data for the server to start
    setupForThreads();

    int pipe_fd;
    char buf[BUFSIZ];

    /* create the FIFO (named pipe) */
    mkfifo(FIFO_FILE, 0666);

    printf("Server ON.\n");

    signal(SIGTERM, handle_server_sigterm); // Register the SIGTERM signal handler
    signal(SIGINT, handle_server_sigint);   // Register the SIGINT signal handler

    while (true)
    {
        // Continuously read from the pipe
        pipe_fd = open(FIFO_FILE, O_RDONLY);
        if (read(pipe_fd, buf, BUFSIZ) > 0)
        {
            // Print the received message
            printf("%s\n", buf);
            // Split the message into client_id and command
            char *saveptr1;
            char *client_name = strtok_r(buf, ": ", &saveptr1);
            char *command = strtok_r(NULL, ": ", &saveptr1);
            // Check what action the client wants to do
            if (strcmp(command, "C") == 0)
            {
                // This is for when a new client wants to register
                bool registered = false;
                // Check if the name is already taken
                registered = registerClient(client_name);
                char message[BUFSIZ];
                message[0] = '\0';
                // Send a message to the client if the registration was successful or not
                if (registered)
                {
                    strcpy(message, "registered");
                }
                else
                {
                    strcpy(message, "notregistered");
                }

                // Create a unique FIFO for the client
                char client_fifo[BUFSIZ];
                snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                mkfifo(client_fifo, 0666);
                int pipe_fd_response = open(client_fifo, O_WRONLY);
                write(pipe_fd_response, message, strlen(message) + 1);
                close(pipe_fd_response);
            }
            if (strcmp(command, "4L") == 0)
            {
                // This is for when a client wants to get the leaderboard

                // Get the leaderboard string and send it to the client
                char *players_string = getLeaderBoard();

                // Create a unique FIFO for the client
                char client_fifo[BUFSIZ];
                snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                mkfifo(client_fifo, 0666);

                int pipe_fd_response = open(client_fifo, O_WRONLY);
                write(pipe_fd_response, players_string, strlen(players_string) + 1);
                close(pipe_fd_response);
                free(players_string);
                players_string = NULL;
            }
            if (strncmp(command, "NG", 2) == 0)
            {
                // This is for when a client wants to create a new game
                char *command_copy = strdup(command);
                char *token = strtok_r(command_copy, ",", &saveptr1);

                if (token && strncmp(token, "NG", 2) == 0)
                {
                    char *num_players_string = strtok_r(NULL, ",", &saveptr1);
                    char *num_rounds_string = strtok_r(NULL, ",", &saveptr1);
                    if (num_players_string == NULL || num_rounds_string == NULL)
                    {
                        printf("Error: Invalid command format\n");
                        continue;
                    }
                    int players_num = atoi(num_players_string);
                    int num_rounds = atoi(num_rounds_string);
                    createNewGameServer(players_num, num_rounds);
                }
                free(command_copy);
                command_copy = NULL;
            }
            if (strcmp(command, "SW") == 0)
            {
                // This is for when a client wants to list the waiting games
                char *games_string = listWaitingGames(client_name);

                char client_fifo[BUFSIZ];
                snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                mkfifo(client_fifo, 0666);
                int pipe_fd_response = open(client_fifo, O_WRONLY);
                write(pipe_fd_response, games_string, strlen(games_string) + 1);
                close(pipe_fd_response);
                free(games_string);
                games_string = NULL;
            }
            if (strncmp(command, "JG", 2) == 0)
            {
                // This is for when a client wants to join a game
                char *command_copy = strdup(command);
                char *token = strtok_r(command_copy, ",", &saveptr1);

                if (token && strncmp(token, "JG", 2) == 0)
                {
                    char *game_id_string = strtok_r(NULL, ",", &saveptr1);
                    char response[BUFSIZ];
                    response[0] = '\0';
                    // Check if it is possible to join the game
                    // Send a message to the client if the join was successful or not
                    if (joinGame(atoi(game_id_string), client_name))
                    {
                        strcat(response, "joined");
                    }
                    else
                    {
                        strcat(response, "notjoined");
                    }

                    char client_fifo[BUFSIZ];
                    snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                    mkfifo(client_fifo, 0666);
                    int pipe_fd_response = open(client_fifo, O_WRONLY);
                    write(pipe_fd_response, response, strlen(response) + 1);
                    close(pipe_fd_response);
                }
                free(command_copy);
                command_copy = NULL;
            }
            if (strcmp(command, "SG") == 0)
            {
                // This is for when a client wants to get the list of games that he is joined
                char *games_string = listGames(client_name);

                char client_fifo[BUFSIZ];
                snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                mkfifo(client_fifo, 0666);
                int pipe_fd_response = open(client_fifo, O_WRONLY);
                write(pipe_fd_response, games_string, strlen(games_string) + 1);
                close(pipe_fd_response);
                free(games_string);
                games_string = NULL;
            }
            if (strncmp(command, "GI", 2) == 0)
            {
                // This is for when a client wants to get the information about a game
                char *command_copy = strdup(command);
                char *token = strtok_r(command_copy, ",", &saveptr1);

                if (token && strncmp(token, "GI", 2) == 0)
                {
                    char *game_id_string = strtok_r(NULL, ",", &saveptr1);
                    if (game_id_string == NULL)
                    {
                        printf("Error: Invalid command format\n");
                        continue;
                    }
                    int game_id = atoi(game_id_string);
                    char response[BUFSIZ];
                    response[0] = '\0';

                    // Get the game info string and send it to the client
                    char *game_info = getGameInfo(game_id);
                    // If the game not started yet send that instead of the game info string
                    if (strcmp(game_info, "notstarted") == 0)
                    {
                        strcat(response, "notstarted");
                    }
                    else
                    {
                        strncat(response, game_info, BUFSIZ - strlen(response) - 1);
                    }
                    free(game_info);
                    game_info = NULL;

                    char client_fifo[BUFSIZ];
                    snprintf(client_fifo, sizeof(client_fifo), "/tmp/my_fifo_response_%s", client_name);
                    mkfifo(client_fifo, 0666);
                    int pipe_fd_response = open(client_fifo, O_WRONLY);
                    write(pipe_fd_response, response, strlen(response) + 1);
                    close(pipe_fd_response);
                }
                free(command_copy);
                command_copy = NULL;
            }
            if (strncmp(command, "MD", 2) == 0)
            {
                // This is for when a client wants to make a decision
                char *command_copy = strdup(command);
                char *token = strtok_r(command_copy, ",", &saveptr1);
                // Get all necessary information from the command string and call the makeDecision function
                if (token && strncmp(token, "MD", 2) == 0)
                {
                    char *game_id_string = strtok_r(NULL, ",", &saveptr1);
                    char *decision_string = strtok_r(NULL, ",", &saveptr1);
                    makeDecision(game_id_string, decision_string, client_name);
                }
            }
        }

        close(pipe_fd);
    }
}

// --------------------------------------------------------
// ---------------------- CLIENT --------------------------
// --------------------------------------------------------

int pipe_fd;
const char *client_id;

// Handle SIGINT signal (Ctrl+C)
void handle_client_sigint(int sig)
{
    clear();
    printw("Quitting...\n");
    refresh();
    sleep(1);
    endwin();
    system("reset");
    exit(0);
}
// Handle SIGTERM signal (kill)
void handle_client_sigterm(int sig)
{
    clear();
    printw("Quitting...\n");
    refresh();
    exit(1);
}

// Handle SIGPIPE signal. This occurs when the server is closed while the client is running.
void handle_sigpipe(int sig)
{
    clear();
    printw("Connection to server failed.\n");
    printw("Quitting...\n");
    refresh();
    sleep(2);
    endwin();
    exit(1);
}

// This function is used to send a command to the server
void commandSender(const char *command)
{
    char message[BUFSIZ + 256];

    // Cat the command to the client id so the server knows which client sent the command
    snprintf(message, sizeof(message), "%s: %s", client_id, command);

    // Write the formatted message to the pipe
    write(pipe_fd, message, strlen(message) + 1);
}

// This function is used to read the response from the server. It is called every time a client expects a response.
char *readFromServer(void)
{
    int pipe_fd_response;
    char buf[BUFSIZ];
    char *message = malloc(BUFSIZ * sizeof(char));
    if (message == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Construct the name of the dedicated pipe using the client's ID
    char fifo_file_response[BUFSIZ];
    sprintf(fifo_file_response, "/tmp/my_fifo_response_%s", client_id);

    // Open the dedicated pipe for reading
    pipe_fd_response = open(fifo_file_response, O_RDONLY);
    if (read(pipe_fd_response, buf, BUFSIZ) > 0)
    {
        // Since this is a dedicated pipe, we don't need to check if the message is for this client
        strcpy(message, buf);
    }
    close(pipe_fd_response);
    return message;
}

// This function checks if the client id is valid
int is_valid_id(const char *id)
{
    // The client id must be a maximum of 5 characters and contain only numbers and letters
    if (strlen(id) > 5)
    {
        return 0;
    }
    while (*id)
    {
        if (!isalnum((unsigned char)*id))
        {
            return 0;
        }
        id++;
    }
    return 1;
}

// This function dispays the menu and handles the user input
void showMenu(void)
{
    clear(); // Clear the console
    printw("1 - Create a new game\n");
    printw("2 - Current games\n");
    printw("3 - Waiting room\n");
    printw("4 - Show leaderboard\n");
    printw("\n");
    printw("q - Quit\n");
    refresh(); // Refresh the screen to apply changes
    bool quit = false;
    do
    {
        // Wait for a key press
        char command = (char)getch();
        // Based on the key pressed do the corresponding action
        switch (command)
        {
        case '1':
            createNewGame();
            quit = true;
            break;
        case '2':
            quit = true;
            showCurrentGames();
            break;
        case '3':
            quit = true;
            displayWaitingRooms(false, 0, NULL);
            break;
        case '4':
            quit = true;
            showLeaderboard();
            break;
        case 'q':
            handle_client_sigint(0);
            quit = true;
            break;
        default:
            break;
        }
    } while (!quit);
}

// This is the option 3 from the menu. It displays the waiting rooms.
void displayWaitingRooms(bool prevJoined, int prevJoinedId, bool cantjoin)
{
    clear();
    printw("Here's the list of games you can join:\n");
    int game_ids[BUFSIZ];
    int games_count = 0;

    usleep(100000);
    // Send the command to the server to get the list of waiting games
    commandSender("SW");
    // Read the response from the server using the readFromServer function
    char *message = readFromServer();
    char *saveptr1;
    char *token = strtok_r(message, ",", &saveptr1);
    while (token != NULL)
    {
        game_ids[games_count] = atoi(token);
        games_count++;
        token = strtok_r(NULL, ",", &saveptr1);
    }
    // If the string is empty there are no games to join
    if (games_count == 0)
    {
        printw("\n");
        printw("Sorry. There are no games you can join");
        printw("\n");
    }
    else
    {
        printw("\n");
    }
    // Print the list of games, assign a number key to each game
    for (int i = 0; i < games_count; i++)
    {
        printw("%d - Game #%d\n", i + 1, game_ids[i]);
    }
    // Check if the client tried to join a game and display a message accordingly
    // If there client already wanted to join the game the displaying is different
    if (prevJoined)
    {
        // check if the previous join was successful or not
        if (cantjoin == 1)
        {
            printw("\n");
            printw(">>> You cannot join Game #%d <<<\n", prevJoinedId);
        }
        else if (cantjoin == 0)
        {
            printw("\n");
            printw(">>> You joined Game #%d <<<\n", prevJoinedId);
        }
    }
    printw("\n");
    printw("b - Back\n");
    refresh();
    bool quit = false;
    free(message);
    message = NULL;
    do
    {
        char command = (char)getch();
        if (command == 'b')
        {
            showMenu();
            quit = true;
        }
        int commandInt = command - '0';
        // Check if the key pressed is a number key and if it is a valid number key for a game
        if (commandInt >= 1 && commandInt <= games_count)
        {
            char commandString[BUFSIZ];
            snprintf(commandString, sizeof(commandString), "JG,%d", game_ids[commandInt - 1]);
            commandSender(commandString);
            usleep(100000);
            message = readFromServer();
            // Check if the join was successful or not
            // According to the response call the function again with the proper parameters
            if (strcmp(message, "joined") == 0)
            {
                free(message);
                message = NULL;
                quit = true;
                displayWaitingRooms(true, game_ids[commandInt - 1], 0);
            }
            else if (strcmp(message, "notjoined") == 0)
            {
                free(message);
                message = NULL;
                quit = true;
                displayWaitingRooms(true, game_ids[commandInt - 1], 1);
            }
        }
    } while (!quit);
}

// This function is used to display the current state of the game when a player enters
void displayGame(int game_id)
{
    clear();
    usleep(100000);
    char commandString[BUFSIZ];
    // Send the command to the server to get the game info with the ID of the game
    snprintf(commandString, sizeof(commandString), "GI,%d", game_id);
    commandSender(commandString);
    char *message = readFromServer();
    // Check if the game is started or not
    if (strcmp(message, "notstarted") == 0)
    {
        printw(">>> Game #%d has not started yet <<<\n", game_id);
        printw("\n");
        printw("r - Refresh\n");
        printw("b - Back\n");
        refresh();
        bool quit = false;
        free(message);
        message = NULL;
        do
        {
            char command = (char)getch();
            if (command == 'b')
            {
                quit = true;
                showMenu();
            }
            if (command == 'r')
            {
                quit = true;
                displayGame(game_id);
            }
        } while (!quit);
    }
    // This part is for converting the game info string to a Game struct

    // Make a copy of the message to avoid modifying the original string
    char *messageCopy = strdup(message);
    if (messageCopy == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    char *token;

    // Create Game struct
    Game currentGame;

    // Parse number of players
    token = strtok(messageCopy, ";");
    token = strtok(NULL, ";");
    currentGame.num_players = atoi(token);

    // Parse player names
    for (int i = 0; i < currentGame.num_players; i++)
    {
        if (i == currentGame.num_players - 1)
        {
            token = strtok(NULL, ";");
        }
        else
        {
            token = strtok(NULL, ",");
        }
        currentGame.playerNames[i] = strdup(token);
    }

    // Parse current round and max rounds
    token = strtok(NULL, ",");
    currentGame.current_round = atoi(token);
    token = strtok(NULL, ";");
    currentGame.num_rounds = atoi(token);

    // Parse player scores
    for (int i = 0; i < currentGame.num_players; i++)
    {
        if (i == currentGame.num_players - 1)
        {
            token = strtok(NULL, ";");
        }
        else
        {
            token = strtok(NULL, ",");
        }
        currentGame.playerScores[i] = atoi(strdup(token));
    }

    // Parse round data
    for (int i = 0; i < currentGame.current_round; i++)
    {
        if (i == currentGame.current_round - 1)
        {
            token = strtok(NULL, ";");
        }
        else
        {
            token = strtok(NULL, ",");
        }
        currentGame.roundData[i] = strdup(token);
    }
    free(messageCopy);
    messageCopy = NULL;

    // The displaying starts here

    // Calculate the length of the separation line
    int sepLineLength = 8 * (currentGame.num_players) + 9;

    // Create the separation line
    char sepLine[sepLineLength + 1]; // +1 for the null terminator
    for (int i = 0; i < sepLineLength; i++)
    {
        sepLine[i] = '-';
    }
    sepLine[sepLineLength] = '\0';

    if (currentGame.current_round > currentGame.num_rounds)
    {
        // If the game is finished display the finished game
        printw("Game #%d - Finished\n", game_id);
        printw("\n%s\n", sepLine);
        printw("| Round |");
        // Display player names
        for (int i = 0; i < currentGame.num_players; i++)
        {
            printw(" %-5s |", currentGame.playerNames[i]);
        }
        printw("\n%s\n", sepLine);
        refresh();
        // Display round data
        for (int i = 0; i < currentGame.num_rounds; i++)
        {
            printw("| %5d |", i + 1);
            // Tokenize the roundData string to extract each player's decision
            for (int j = 0; j < currentGame.num_players; j++)
            {
                if (currentGame.roundData[i][j] == 'n')
                {
                    printw(" %5c |", ' ');
                }
                else
                {
                    printw(" %5c |", currentGame.roundData[i][j]);
                }
            }
            printw("\n%s\n", sepLine);
        }

        // Display player scores
        printw("\n%s\n", sepLine);
        printw("|  Pts  |");
        for (int i = 0; i < currentGame.num_players; i++)
        {
            printw(" %5d |", currentGame.playerScores[i]);
        }
        printw("\n%s\n", sepLine);

        // Display the winner
        printw("\n>>> Game over! Winner(s): ");
        // In order to print the winners in the correct order we need to find the max score
        // find max score
        int maxScore = 0;
        for (int i = 0; i < currentGame.num_players; i++)
        {
            if (currentGame.playerScores[i] > maxScore)
            {
                maxScore = currentGame.playerScores[i];
            }
        }
        // print winners, only put comma if multiple winners
        int winnerCount = 0;
        for (int i = 0; i < currentGame.num_players; i++)
        {
            if (currentGame.playerScores[i] == maxScore)
            {
                winnerCount++;
            }
        }
        for (int i = 0; i < currentGame.num_players; i++)
        {
            if (currentGame.playerScores[i] == maxScore)
            {
                printw("%s", currentGame.playerNames[i]);
                winnerCount--;
                if (winnerCount > 0)
                {
                    printw(", ");
                }
            }
        }
        printw("<<<\n");

        // Display the options
        printw("\nb - Back\n");
        refresh();
        bool quit = false;
        do
        {
            char command = (char)getch();
            // Only option from here is to go back to the menu, the player cannot come back here
            if (command == 'b')
            {
                quit = true;
                showMenu();
            }
        } while (!quit);
        return;
    }

    // If the game is not finished display the current state of the game
    printw("Game #%d - Next Round %d/%d\n\n", game_id, currentGame.current_round, currentGame.num_rounds);
    // Display the top border of the table
    printw("\n%s\n", sepLine);
    printw("| Round |");
    refresh();
    // Display player names
    for (int i = 0; i < currentGame.num_players; i++)
    {
        printw(" %-5s |", currentGame.playerNames[i]);
    }
    printw("\n%s\n", sepLine);
    refresh();
    // Display round data
    for (int i = 0; i < currentGame.current_round - 1; i++)
    {
        printw("| %5d |", i + 1);
        // Tokenize the roundData string to extract each players decision
        for (int j = 0; j < currentGame.num_players; j++)
        {
            if (currentGame.roundData[i][j] == 'n')
            {
                printw(" %5c |", ' ');
            }
            else
            {
                printw(" %5c |", currentGame.roundData[i][j]);
            }
        }
        printw("\n%s\n", sepLine);
    }

    // Display player scores
    printw("\n%s\n", sepLine);
    printw("|  Pts  |");
    for (int i = 0; i < currentGame.num_players; i++)
    {
        printw(" %5d |", currentGame.playerScores[i]);
    }
    printw("\n%s\n", sepLine);

    // check if already made a decision
    bool madeDecision = false;
    for (int i = 0; i < currentGame.num_players; i++)
    {
        if (strcmp(currentGame.playerNames[i], client_id) == 0)
        {
            if (currentGame.roundData[currentGame.current_round - 1][i] != 'n')
            {
                madeDecision = true;
                break;
            }
        }
    }
    if (!madeDecision)
    {
        // If not, display the options
        printw("\np - Choose \"Paper\"\n");
        printw("r - Choose \"Rock\"\n");
        printw("s - Choose \"Scissor\"\n\n");
        printw("b - Back\n");
        refresh();

        // Handle user input
        bool quit = false;
        do
        {
            char command = (char)getch();
            if (command == 'b')
            {
                quit = true;
                showMenu();
            }
            if (command == 'p' || command == 'r' || command == 's')
            {
                // Send the decision to the server
                commandString[0] = '\0';
                snprintf(commandString, sizeof(commandString), "MD,%d,%c", game_id, command);
                commandSender(commandString);
                quit = true;
                usleep(100000);
                // After sending the decision restart the function to display the updated game
                displayGame(game_id);
            }
        } while (!quit);
    }
    else
    {
        // check what the player chose
        char playerDecision;
        playerDecision = '\0';
        for (int i = 0; i < currentGame.num_players; i++)
        {
            if (strcmp(currentGame.playerNames[i], client_id) == 0)
            {
                playerDecision = currentGame.roundData[currentGame.current_round - 1][i];
                break;
            }
        }
        printw("\n");
        // Display the player's decision
        if (playerDecision == 'p')
        {
            printw(">>> You have chosen paper <<<\n");
        }
        else if (playerDecision == 'r')
        {
            printw(">>> You have chosen rock <<<\n");
        }
        else if (playerDecision == 's')
        {
            printw(">>> You have chosen scissor <<<\n");
        }

        // Display the options
        printw("\nr - Refresh\n");
        printw("b - Back\n");
        refresh();

        // Handle user input
        bool quit = false;
        do
        {
            char command = (char)getch();
            if (command == 'b')
            {
                // The player can go back to the menu
                quit = true;
                showMenu();
            }
            if (command == 'r')
            {
                // Or refresh the game to see if the other players made their decisions
                quit = true;
                displayGame(game_id);
            }
        } while (!quit);
    }
}

// This function is to display the games that the player is joined
void showCurrentGames(void)
{
    clear();
    printw("Here's the list of games you can play:\n");
    printw("\n");
    commandSender("SG");
    char *message = readFromServer();
    char *saveptr1;
    char *token = strtok_r(message, ",", &saveptr1);
    int game_ids[BUFSIZ];
    int games_count = 0;
    while (token != NULL)
    {
        game_ids[games_count] = atoi(token);
        games_count++;
        token = strtok_r(NULL, ",", &saveptr1);
    }
    if (games_count == 0)
    {
        printw("Sorry. There are no games you can play");
        printw("\n");
    }
    for (int i = 0; i < games_count; i++)
    {
        printw("%d - Game #%d\n", i + 1, game_ids[i]);
    }
    printw("\n");
    printw("b - Back\n");
    refresh();
    bool quit = false;
    free(message);
    message = NULL;
    do
    {
        char command = (char)getch(); // Wait for a key press
        if (command == 'b')
        {
            quit = true;
            showMenu();
        }
        int commandInt = command - '0';
        // Check if the key pressed is a number key and if it is a valid number key for a game
        // If yes step into the game
        if (commandInt >= 1 && commandInt <= games_count)
        {
            quit = true;
            displayGame(game_ids[commandInt - 1]);
        }
    } while (!quit);
}

// This function is used to create a new game
void createNewGame(void)
{
    clear();
    printw("2:5 - How many players will play the game? min: 2 - max: 5\n");
    printw("\n");
    printw("b - Back\n");
    refresh();
    bool quit = false;
    int playersn = 0;
    bool gamecreated = false;
    bool secondquestion = false;
    do
    {
        char command = (char)getch();
        // If the player presses b check witch state of the game creation he is in
        if (command == 'b')
        {
            // If they already entered the player number go back to the first question
            if (secondquestion)
            {
                createNewGame();
                quit = true;
            }
            else
            {
                showMenu();
                quit = true;
            }
        }
        // If the number of entered players is valid go to the next question
        else if (playersn == 0 && atoi(&command) >= 2 && atoi(&command) <= 5 && !gamecreated)
        {
            secondquestion = true;
            playersn = atoi(&command);
            clear();
            printw("3:5 - How many rounds will the game have? min: 3 - max: 5\n");
            printw("\n");
            printw("b - Back\n");
            refresh();
        }
        // If the game way created display a message about it
        else if (playersn != 0 && atoi(&command) >= 3 && atoi(&command) <= 5 && !gamecreated)
        {
            int rounds = atoi(&command);
            char commandString[BUFSIZ];
            snprintf(commandString, sizeof(commandString), "NG,%d,%d", playersn, rounds);
            commandSender(commandString);
            secondquestion = false;
            clear();
            printw("Game created, you can join from the menu\n");
            printw("\n");
            printw("b - Back\n");
            refresh();
            gamecreated = true;
        }
    } while (!quit);
}

// This function is used to show the leaderboard
void showLeaderboard(void)
{
    // Ask the server for the leaderboard
    commandSender("4L");
    clear();
    printw("Leaderboard\n");
    printw("\n");
    refresh();
    char currentPlayer[BUFSIZ];
    strcpy(currentPlayer, client_id);
    char *message = readFromServer();
    // Call the parseMessage function to parse the leaderboard string
    parseMessage(message, currentPlayer);
    bool quit = false;
    do
    {
        char command = (char)getch(); // Wait for a key press
        switch (command)
        {
        case 'b':
            quit = true;
            showMenu();
            break;
        default:
            break;
        }
    } while (!quit);
}

// This function is used to get out the information about a game to be able to sort it properly.
void parseMessage(char *message, char *currentPlayer)
{
    Player currentPlayers[MAX_PLAYERS];
    int playerCount = 0;

    char *saveptr1, *saveptr2;
    char *playerToken = strtok_r(message, ";", &saveptr1);
    while (playerToken != NULL)
    {
        char *name = strtok_r(playerToken, ",", &saveptr2);
        char *score = strtok_r(NULL, ",", &saveptr2);
        char *matches = strtok_r(NULL, ",", &saveptr2);

        strncpy(currentPlayers[playerCount].name, name, sizeof(currentPlayers[playerCount].name));
        currentPlayers[playerCount].score = atoi(score);
        currentPlayers[playerCount].matches = atoi(matches);

        playerCount++;

        playerToken = strtok_r(NULL, ";", &saveptr1);
    }
    // Send the information to the printLeaderboard function
    printLeaderboard(currentPlayers, playerCount, currentPlayer);
    free(message);
    message = NULL;
}

void printLeaderboard(Player currentPlayers[], int size, char *currentPlayer)
{
    // Sort the players based on the score and name
    qsort(currentPlayers, (size_t)size, sizeof(Player), compare);
    // Display all information in the correct format
    printw("-----------------------------------\n");
    printw("| Rank | Player | Score | Matches |\n");
    printw("-----------------------------------\n");
    // Check the number of players
    int topPlayers = size < 3 ? size : 3;
    // Display the top 3 players first. If the current player is not in the top 3 display it after the top 3.
    for (int i = 0; i < topPlayers; i++)
    {
        printw("|%s  %2d | %6s | %5d | %7d |\n", strcmp(currentPlayers[i].name, currentPlayer) == 0 ? "*" : " ", i + 1, currentPlayers[i].name, currentPlayers[i].score, currentPlayers[i].matches);
        printw("-----------------------------------\n");
    }
    // If the current player is not in the top 3 display it after the top 3.
    if (strcmp(currentPlayers[topPlayers - 1].name, currentPlayer) != 0)
    {
        for (int i = topPlayers; i < size; i++)
        {
            if (strcmp(currentPlayers[i].name, currentPlayer) == 0)
            {
                printw("\n");
                printw("-----------------------------------\n");
                printw("|* %3d | %6s | %5d | %7d |\n", i + 1, currentPlayers[i].name, currentPlayers[i].score, currentPlayers[i].matches);
                printw("-----------------------------------\n");
                break;
            }
        }
    }
    printw("\n");
    printw("b - Back\n");
    refresh();
}

// This function is used to compare two players based on their score and name
int compare(const void *a, const void *b)
{
    Player *playerA = (Player *)a;
    Player *playerB = (Player *)b;

    if (playerA->score != playerB->score)
        return playerB->score - playerA->score;

    return strcmp(playerA->name, playerB->name);
}

// This function initializes the client
void client(const char *id)
{
    // Setup for ncurses library
    initscr();            // Initialize the ncurses library
    cbreak();             // Disable line buffering
    noecho();             // Don't echo the typed characters
    keypad(stdscr, TRUE); // Enable function keys and arrow keys

    pipe_fd = open(FIFO_FILE, O_WRONLY);
    client_id = id;

    signal(SIGINT, handle_client_sigint);   // Register the SIGINT signal handler
    signal(SIGTERM, handle_client_sigterm); // Register the SIGTERM signal handler
    signal(SIGPIPE, handle_sigpipe);        // Register the SIGPIPE signal handler

    // Try to connect to the server
    commandSender("C");
    usleep(10000);
    // check response
    char *message = readFromServer();
    if (strcmp(message, "registered") == 0)
    {
        free(message);
        message = NULL;
        showMenu();
    }
    // If there is already a client with this name the player cant join.
    else
    {
        clear();
        printw("Error: Client with this name already exists\n");
        refresh();
        free(message);
        message = NULL;
        sleep(1);
        exit(1);
    }
}
