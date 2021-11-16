#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>

#define BOARD_WIDTH 100
#define BOARD_HEIGHT 30
#define MICROSECONDS_PER_SECOND 1000000
#define FRAMES_PER_SECOND 60
#define SECONDS_FOR_BALL_TO_TRAVEL_ACROSS_BOARD 2
#define PADDLE_WIDTH 3

struct ball
{
    float x;
    float y;
    float velocity_x;
    float velocity_y;
};

struct game
{
    int player_paddle_y;
    int cpu_paddle_y;
    struct ball ball;
    char board[BOARD_WIDTH * BOARD_HEIGHT + BOARD_WIDTH]; // padding added to prevent segfault d/t off-by-one error
    int score;
    struct termios *terminal; // We need this to restore terminal input settings when we exit.
};

struct thread_args
{
    struct game *game;
    struct termios *terminal;
};

float _sqrt(float a)
{
    // Newton's method
    float x = 5;
    for (int i = 0; i < 10; i++)
    {
        int new_x = 0.5 * (x + (a / x));
        x = new_x;
    }
    return x;
}

void print_intro(void)
{
    printf("\nWelcome to...\n\n\n");
    printf("\n\
    \tPPPPPP      OOOO     NNNN       NN      OOOO     \n\
    \tPPPPPPP    OOOOOO    NNNNN      NN    OOOOOOOO   \n\
    \tPPPPPPPP  OO    OO   NN NNN     NN   OO      OO  \n\
    \tPP   PPP OO      OO  NN  NNN    NN  OO       OO  \n\
    \tPPPPPPP  OO      OO  NN   NNN   NN  OO           \n\
    \tPPPPPP   OO      OO  NN    NNN  NN  OO    OOOOO  \n\
    \tPP        OO    OO   NN     NNN NN   OO      OO  \n\
    \tPP         OOOOOO    NN      NNNNN    OOOOOOOO   \n\
    \tPP          OOOO     NN       NNNN      OOOO     \n\
    ");
    printf("\n\n\nCommands:\n\tW: move up\n\tS: move down\n\tQ: quit\n\nGood Luck!\n\n");
    sleep(2);
    
    // Must flush to prevent buffering of short print statements.
    printf("Starting in 3...");
    fflush(stdout);
    sleep(1);
    printf(" 2...");
    fflush(stdout);
    sleep(1);
    printf(" 1...");
    fflush(stdout);
    sleep(1);
    printf(" GO!\n\n");
    fflush(stdout);
    sleep(1);
}

void print_outro(struct game *game)
{
    // We can only lose since CPU is unbeatable.
    sleep(1);
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nOops! You lost.\n\nYour score was: %d\n", game->score);
    sleep(3);
    if (game->score == 0)
    {
        printf("\nReally? Not a single volley?\n\n");
    }
    else if (game->score < 100)
    {
        printf("\nNot bad! Not good either....\n\n");
    }
    else
    {
        printf("\nYou're getting pretty good!\n\n");
    }
    sleep(3);
    printf("Goodbye.\n");
    sleep(1);
    tcsetattr(STDIN_FILENO, TCSANOW, game->terminal); // reset terminal settings.
    exit(0);
}

void draw_blank_board(struct game *game)
{
    // NOTE:
    // We naively clear entire board every frame. A more sophisticated
    // approach would be clearing only the previous positions of the paddles
    // and ball every frame. This would decrease time complexity from O(n),
    // where n is the size of the board, to O(1).
    //
    // I have not run into any performance constraints yet so I don't think its
    // necessary, but I could see it becoming an issue if we push frame rate really
    // high or make the board super massive. At 60 fps and a board size of 100x30
    // everything is fine.
    for (int i = 0; i < BOARD_HEIGHT * BOARD_WIDTH; i++)
    {
        if (i % BOARD_WIDTH != 0)
        {
            if ((i - BOARD_WIDTH < 0 || i - (BOARD_HEIGHT * BOARD_WIDTH - BOARD_WIDTH) >= 0)) // Top and bottom row
            {
                game->board[i] = '=';
            }
            else
            {
                game->board[i] = ' ';
            }
        }
        else // We've reached the end of a row.
        {
            game->board[i] = '\n';
        }
    }

    // Literal corner cases - we don't want to paint '=' in the corners
    // since paddles can travel all the way to corner. If we don't like this
    // aesthetic we can change the paddle collision bounds checking and remove
    // these corner cases to paint '=' all the way across the top and bottom.
    game->board[1] = ' ';
    game->board[BOARD_WIDTH - 1] = ' ';
    game->board[BOARD_WIDTH * (BOARD_HEIGHT - 1) + 1] = ' ';
    game->board[BOARD_WIDTH * (BOARD_HEIGHT)-1] = ' ';
}

// Process input from the keyboard. Return 1 on quit signal, otherwise 0
// for successful input. On valid input, move player paddle accordingly.
int process_input(struct game *game, char direction)
{
    if (direction == 'w') // up
    {
        game->player_paddle_y = game->player_paddle_y > 1 ? game->player_paddle_y - 1 : game->player_paddle_y;
    }
    else if (direction == 's') // down
    {
        game->player_paddle_y = game->player_paddle_y < BOARD_HEIGHT - 2 ? game->player_paddle_y + 1 : game->player_paddle_y;
    }
    else if (direction == 'q') // quit
    {
        return 1;
    }
    return 0;
}

// Render game to terminal
void paint(struct game *game)
{
    // To render the game objects, we flatten their coordinates
    // to index into our serialized game board character buffer.
    int player_y = game->player_paddle_y * BOARD_WIDTH + 1;
    int cpu_y = (game->cpu_paddle_y + 1) * BOARD_WIDTH - 1;
    int ball_x = (int)(game->ball.x + 0.5); // Snap float position to nearest integer
    int ball_y = (int)(game->ball.y + 0.5);

    // This moves the cpu to wherever the ball is. It is but an
    // illusion, since in reality the ball rebounds from the cpu side
    // regardless of where the cpu paddle is. We don't want to extinguish
    // players hope that they may win someday!
    if (ball_y > 0 && ball_y < BOARD_HEIGHT - 1)
    {
        cpu_y = (ball_y + 1) * BOARD_WIDTH - 1;
    }

    draw_blank_board(game);

    // update with new positions
    game->board[player_y] = '|';
    game->board[player_y - BOARD_WIDTH] = '|';
    game->board[player_y + BOARD_WIDTH] = '|';
    game->board[cpu_y] = '|';
    game->board[cpu_y - BOARD_WIDTH] = '|';
    game->board[cpu_y + BOARD_WIDTH] = '|';
    game->board[(ball_y * BOARD_WIDTH) + ball_x] = 'O';

    // paint to screen
    printf("\n\n%s\nScore: %d\n", game->board, game->score);
}

void update_ball_position(struct game *game)
{
    int new_x = (int)(game->ball.x + game->ball.velocity_x + 0.5);
    int new_y = (int)(game->ball.y + game->ball.velocity_y + 0.5);

    if (new_x == 1) // ball reached player side
    {
        int paddle_offset = PADDLE_WIDTH / 2 + 1; // give some forgiveness to the player by adding 1 to paddle offset

        if (game->player_paddle_y - new_y <= paddle_offset && game->player_paddle_y - new_y >= -paddle_offset) // player hit ball
        {
            if (new_y > game->player_paddle_y) // hit ball with bottom of paddle
            {
                game->ball.velocity_y += 0.05;
            }
            else if (new_y < game->player_paddle_y) // hit ball with top of paddle
            {
                game->ball.velocity_y -= 0.05;
            }
            else // hit ball with center of paddle
            {
                // we add a little noise to keep it spicy even when the player hit perfectly.
                float noisy_redirection = game->score % 2 == 0 ? 0.01 : -0.01;
                noisy_redirection = game->score % 3 == 0 ? noisy_redirection * 2 : noisy_redirection * 3;
                game->ball.velocity_y = game->ball.velocity_y + noisy_redirection;
            }
            game->ball.velocity_x = game->ball.velocity_x * (-1);
            game->score++;
            printf("\a");
        }
        else // player missed ball
        {
            print_outro(game); // quit game
        }
    }
    else if (new_x == BOARD_WIDTH - 1) // ball reached cpu side
    {
        game->ball.velocity_x = game->ball.velocity_x * (-1); // cpu can't lose!!
        printf("\a");
    }

    // handling wall rebounds
    if (new_y == 0) // top wall
    {
        game->ball.velocity_y = game->ball.velocity_y * (-1);
    }
    else if (new_y == BOARD_HEIGHT - 1) // bottom wall
    {
        game->ball.velocity_y = game->ball.velocity_y * (-1);
    }

    // finally we update the ball position
    game->ball.x += game->ball.velocity_x;
    game->ball.y += game->ball.velocity_y;
}

// Refreshes gamestate and rerenders game. Returns 1 on quit signal, otherwise 0.
int refresh(struct game *game)
{
    // ball updates occur here, cpu paddle updates occur
    // in paint, and player paddle updates occur in separate
    // thread dedicated to input handling.
    update_ball_position(game);
    paint(game);
    return 0;
}

// Main routine for input handling thread. Processes keyboard
// input and updates paddle position.
//
// Must pass in a handle to a thread_args struct containing a game and
// termios handle.
void *input_handler(void *arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    struct game *game = args->game;
    struct termios *terminal = args->terminal;
    while (1)
    {
        // process keyboard input
        char input = getchar();
        int result = process_input(game, input);
        if (result == 1) // handling quit signal.
        {
            tcsetattr(STDIN_FILENO, TCSANOW, terminal); // reset terminal before exit.
            exit(0);
        }
    }
}

int main(int argc, char *argv[])
{
    // This disables canonical mode and echoing from terminal
    // so keyboard inputs are not buffered before being
    // written to stdin and they are not echoed to stdout.
    // This can mess up your terminal if you don't reset the
    // settings before exiting the program, so always make sure
    // to reset! If you forget or the program crashes unexpectedly,
    // you can always close and reopen the terminal to reset it.
    static struct termios old_terminal, new_terminal;
    tcgetattr(STDIN_FILENO, &old_terminal);
    new_terminal = old_terminal;
    new_terminal.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);

    // intialize game objects
    struct game game;
    struct ball ball;
    ball.x = BOARD_WIDTH / 2;
    ball.y = BOARD_HEIGHT / 2;
    ball.velocity_x = -((float)BOARD_WIDTH / (float)(SECONDS_FOR_BALL_TO_TRAVEL_ACROSS_BOARD * FRAMES_PER_SECOND));
    ball.velocity_y = 0; // Ball will travel horizontal at first
    game.board[BOARD_WIDTH * BOARD_HEIGHT];
    game.player_paddle_y = BOARD_HEIGHT / 2;
    game.cpu_paddle_y = BOARD_HEIGHT / 2;
    game.ball = ball;
    game.score = 0;
    game.terminal = &old_terminal;

    draw_blank_board(&game);

    // Fork new thread for handling keyboard input.
    //
    // NOTE: In the future we could add a lock surrounding player paddle
    // updates. Right now there could be a race condition leading to a
    // keyboard input getting skipped or the paddle being rendered a frame
    // late. Not the end of the world, but could be annoying if it happens
    // all the time.
    pthread_t thread_2;
    struct thread_args thread_args;
    thread_args.game = &game;
    thread_args.terminal = &old_terminal;
    int rc = pthread_create(&thread_2, NULL, input_handler, &thread_args);
    if (rc != 0) // return code 0 == success, else error
    {
        printf("Failed to fork thread. Error code: %d", rc);
        tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal); // always remember to reset terminal settings before exit!
        exit(1);
    }

    // Right now the sleep interval is fixed to a set duration, regardless
    // of how long the game loop takes to execute. If the game were to be
    // more demanding, for instance actaully rendering a bitmap that is
    // potentially thousands of pixels in size, we should make our sleep timing
    // variable each frame depending on how long the game loop actually took.
    int microseconds_per_frame = MICROSECONDS_PER_SECOND / FRAMES_PER_SECOND;

    // start game loop
    print_intro();
    while (1)
    {
        if (refresh(&game) == 1) // quit command is sent
        {
            break;
        }
        usleep(microseconds_per_frame);
    }
    printf("\n\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal); // restore previous terminal settings
    return 0;
}