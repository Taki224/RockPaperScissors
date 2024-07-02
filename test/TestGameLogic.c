#include "unity.h"
#include "gameLogic.h"
#include <pthread.h>

void setUp(void)
{
    // Reset the players and games arrays
    memset(players, 0, sizeof(players));
    memset(games, 0, sizeof(games));
    num_players = 0;
    game_num = 1;
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    usleep(1000);
}

void tearDown(void)
{
    // Stop the threads of all running games
    for (int i = 0; i < num_threads; i++) {
        pthread_cancel(thread_ids[i]);
        pthread_join(thread_ids[i], NULL);
    }
    num_threads = 0;
}


void test_valid_id(void)
{
    bool result = is_valid_id("foo");
    TEST_ASSERT_TRUE(result);
    result = is_valid_id("pino");
    TEST_ASSERT_TRUE(result);
}

void test_invalid_id(void)
{
    bool result = is_valid_id("fooooo");
    TEST_ASSERT_FALSE(result);
}

void test_register_user(void)
{
    char client_name[] = "foo";
    registerClient(client_name);

    TEST_ASSERT_TRUE(num_players == 1);
    TEST_ASSERT_TRUE(strcmp(players[0].name, client_name) == 0);

    char client_name2[] = "bar";
    registerClient(client_name2);

    TEST_ASSERT_TRUE(num_players == 2);
    TEST_ASSERT_TRUE(strcmp(players[1].name, client_name2) == 0);
}

void test_create_game(void)
{
    createNewGameServer(2, 2);
    TEST_ASSERT_TRUE(game_num == 2);
    TEST_ASSERT_TRUE(games[1].num_players == 2);
    TEST_ASSERT_TRUE(games[1].num_rounds == 2);
    TEST_ASSERT_TRUE(games[1].current_round == 1);
    TEST_ASSERT_TRUE(games[1].num_joined_players == 0);
    TEST_ASSERT_TRUE(games[1].num_current_round == 0);
    TEST_ASSERT_TRUE(games[1].started == false);
    TEST_ASSERT_TRUE(games[1].active == true);
}

void test_create_multiple_games(void)
{
    createNewGameServer(2, 3);
    TEST_ASSERT_TRUE(game_num == 2);
    TEST_ASSERT_TRUE(games[1].num_players == 2);
    TEST_ASSERT_TRUE(games[1].num_rounds == 3);
    TEST_ASSERT_TRUE(games[1].current_round == 1);
    TEST_ASSERT_TRUE(games[1].num_joined_players == 0);
    TEST_ASSERT_TRUE(games[1].num_current_round == 0);
    TEST_ASSERT_TRUE(games[1].started == false);
    TEST_ASSERT_TRUE(games[1].active == true);
    
    createNewGameServer(3, 4);
    TEST_ASSERT_TRUE(game_num == 3);
    TEST_ASSERT_TRUE(games[2].num_players == 3);
    TEST_ASSERT_TRUE(games[2].num_rounds == 4);
    TEST_ASSERT_TRUE(games[2].current_round == 1);
    TEST_ASSERT_TRUE(games[2].num_joined_players == 0);
    TEST_ASSERT_TRUE(games[2].num_current_round == 0);
    TEST_ASSERT_TRUE(games[2].started == false);
    TEST_ASSERT_TRUE(games[2].active == true);
}

void test_join_game(void)
{
    char client_name[] = "foo";
    registerClient(client_name);
   
    createNewGameServer(2, 3);
    
    joinGame(1, client_name);
    
    TEST_ASSERT_TRUE(games[1].num_joined_players == 1);
    TEST_ASSERT_TRUE(strcmp(games[1].playerNames[0], client_name) == 0);
    TEST_ASSERT_TRUE(games[1].playerScores[0] == 0);

    char client_name2[] = "bar";
    registerClient(client_name2);
    joinGame(1, client_name2);
    TEST_ASSERT_TRUE(games[1].num_joined_players == 2);
    TEST_ASSERT_TRUE(strcmp(games[1].playerNames[1], client_name2) == 0);
    TEST_ASSERT_TRUE(games[1].playerScores[1] == 0);
}

void test_join_multiple_games(void){
    
    
    createNewGameServer(2, 3);
    createNewGameServer(3, 3);

    char client_name[] = "foo";
    registerClient(client_name);
    joinGame(1, client_name);
    TEST_ASSERT_TRUE(games[1].num_joined_players == 1);
    TEST_ASSERT_TRUE(strcmp(games[1].playerNames[0], client_name) == 0);
    TEST_ASSERT_TRUE(games[1].playerScores[0] == 0);

    char client_name2[] = "bar";
    registerClient(client_name2);
    joinGame(1, client_name2);
    TEST_ASSERT_TRUE(games[1].num_joined_players == 2);
    TEST_ASSERT_TRUE(strcmp(games[1].playerNames[1], client_name2) == 0);
    TEST_ASSERT_TRUE(games[1].playerScores[1] == 0);

    char client_name3[] = "baz";
    registerClient(client_name3);
    joinGame(2, client_name3);
    TEST_ASSERT_TRUE(games[2].num_joined_players == 1);
    TEST_ASSERT_TRUE(strcmp(games[2].playerNames[0], client_name3) == 0);
    TEST_ASSERT_TRUE(games[2].playerScores[0] == 0);

    char client_name4[] = "qux";
    registerClient(client_name4);
    joinGame(2, client_name4);
    TEST_ASSERT_TRUE(games[2].num_joined_players == 2);
    TEST_ASSERT_TRUE(strcmp(games[2].playerNames[1], client_name4) == 0);
    TEST_ASSERT_TRUE(games[2].playerScores[1] == 0);
}

void test_get_gameinfo_notstarted(void){
    createNewGameServer(2, 3);
    char *gameInfo = getGameInfo(1);
    TEST_ASSERT_TRUE(strcmp(gameInfo, "notstarted") == 0);
    free(gameInfo);
}

void test_get_gameinfo_started(void){
    createNewGameServer(2, 3);
    char client_name[] = "foo";
    joinGame(1, client_name);
    char client_name2[] = "bar";
    joinGame(1, client_name2);
    usleep(10);
    char *gameInfo = getGameInfo(1);
    TEST_ASSERT_TRUE(strcmp(gameInfo, "GI;2;foo,bar;1,3;0,0;nn;") == 0);
    free(gameInfo);
}

void test_leaderboard(void){
    createNewGameServer(2, 3);
    char client_name[] = "foo";
    registerClient(client_name);
    char client_name2[] = "bar";
    registerClient(client_name2);
    char *leaderboard = getLeaderBoard();

    TEST_ASSERT_TRUE(strcmp(leaderboard, "foo,0,0;bar,0,0;") == 0);
    free(leaderboard);
}

void test_make_decesion(void){
    createNewGameServer(2, 3);
    char client_name[] = "foo";
    joinGame(1, client_name);
    char client_name2[] = "bar";
    joinGame(1, client_name2);

    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    char *gameInfo = getGameInfo(1);
    TEST_ASSERT_TRUE(strcmp(gameInfo, "GI;2;foo,bar;1,3;0,0;rn;") == 0);
    free(gameInfo);
}

void test_leaderboard_aftergame(void){
    createNewGameServer(2, 3);
    char client_name[] = "foo";
    registerClient(client_name);
    joinGame(1, client_name);
    char client_name2[] = "bar";
    registerClient(client_name2);
    joinGame(1, client_name2);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    char *leaderboard = getLeaderBoard();
    TEST_ASSERT_TRUE(strcmp(leaderboard, "foo,0,1;bar,3,1;") == 0);
    free(leaderboard);
}

void test_multiple_games_same_time(void){
    char client_name[] = "foo";
    registerClient(client_name);
    char client_name2[] = "bar";
    registerClient(client_name2);
    char client_name3[] = "baz";
    registerClient(client_name3);
    createNewGameServer(2, 3);
    joinGame(1, client_name);
    joinGame(1, client_name2);
    createNewGameServer(2, 3);
    joinGame(2, client_name);
    joinGame(2, client_name3);
    usleep(10);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    makeDecision((char*)"1", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"1", (char*)"p", client_name2);
    usleep(10);
    makeDecision((char*)"2", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"2", (char*)"p", client_name3);
    usleep(10);
    makeDecision((char*)"2", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"2", (char*)"p", client_name3);
    usleep(10);
    makeDecision((char*)"2", (char*)"r", client_name);
    usleep(10);
    makeDecision((char*)"2", (char*)"p", client_name3);
    usleep(10);
    char *leaderboard = getLeaderBoard();
    TEST_ASSERT_TRUE(strcmp(leaderboard, "foo,0,2;bar,3,1;baz,3,1;") == 0);
    free(leaderboard);
}

void test_waiting_rooms(void){
    char client_name[] = "foo";
    registerClient(client_name);
    char client_name2[] = "bar";
    registerClient(client_name2);
    char client_name3[] = "baz";
    registerClient(client_name3);

    createNewGameServer(2, 3);
    char *waitingRooms = listWaitingGames(client_name3);
    TEST_ASSERT_TRUE(strcmp(waitingRooms, "1,") == 0);

    joinGame(1, client_name);
    usleep(10);
    waitingRooms = listWaitingGames(client_name);
    TEST_ASSERT_TRUE(strcmp(waitingRooms, "") == 0);


    joinGame(1, client_name2);
    usleep(10);
    waitingRooms = listWaitingGames(client_name3);
    TEST_ASSERT_TRUE(strcmp(waitingRooms, "") == 0);
    
    free(waitingRooms);
}

void test_games(void){
    char client_name[] = "foo";
    registerClient(client_name);
    
    createNewGameServer(2, 3);
    char *gamesList = listGames(client_name);
    TEST_ASSERT_TRUE(strcmp(gamesList, "") == 0);

    char *waitingGames = listWaitingGames(client_name);
    TEST_ASSERT_TRUE(strcmp(waitingGames, "1,") == 0);

    joinGame(1, client_name);
    
    waitingGames = listWaitingGames(client_name);
    TEST_ASSERT_TRUE(strcmp(waitingGames, "") == 0);

    gamesList = listGames(client_name);
    TEST_ASSERT_TRUE(strcmp(gamesList, "1,") == 0);

    free(gamesList);
    free(waitingGames);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_valid_id);
    RUN_TEST(test_invalid_id);
    RUN_TEST(test_register_user);
    RUN_TEST(test_create_game);
    RUN_TEST(test_create_multiple_games);
    RUN_TEST(test_join_game);
    RUN_TEST(test_join_multiple_games);
    RUN_TEST(test_get_gameinfo_notstarted);
    RUN_TEST(test_get_gameinfo_started);
    RUN_TEST(test_leaderboard);
    RUN_TEST(test_make_decesion);
    RUN_TEST(test_leaderboard_aftergame);
    RUN_TEST(test_multiple_games_same_time);
    RUN_TEST(test_waiting_rooms);
    RUN_TEST(test_games);

    UNITY_END();

    return 0;
}
