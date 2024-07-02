#include "gameLogic.c"
#include "Main.h"

int main(int argc, char *argv[])
{
    if (argc < 2 || (strcmp(argv[1], "--server") != 0 && strcmp(argv[1], "--client") != 0))
    {
        fprintf(stderr, "Usage: %s --server|--client <CLIENT_ID>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--server") == 0)
    {
        server();
    }
    else
    {
        if (argc != 3)
        {
            fprintf(stderr, "Usage: %s --client <CLIENT_ID>\n", argv[0]);
            return 1;
        }
        if (!is_valid_id(argv[2]))
        {
            fprintf(stderr, "Invalid client ID. Must be a maximum of 5 characters and contain only numbers and letters.\n");
            return 1;
        }
        client(argv[2]);
    }

    return 0;
}
