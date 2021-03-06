#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#include "hashmap.h"

#define BOARD_SIDE 3    // side of the board
#define BOARD_LENGTH BOARD_SIDE * BOARD_SIDE    // total length of an array representing a board
#define MAX_DEPTH 20    // the default maximum depth for the DFS algo
#define MAX_LEVEL 20    // the maximum number of OMP levels for recursion
#define COLOR 0         // enable the colors in the terminal output
#define DEBUG 0         // print the debug output

int max_depth;      // maximum dpeth for the dfs algorithm
int best_depth;     // the current depth where a solution has been found
int *best_moves;    // the array containing the current best path
long iter = 0;      // the iterations count


/**
 * Enumeration describing the possible directions
 */
typedef enum direction direction;
enum direction {
    UP, DOWN, LEFT, RIGHT
};

/**
 * Type describing a move with its distance from the correct solution
 */
typedef struct {
    direction dir;
    int pos;
    int manhattan_distance;
} move;


/**
 * Print the square board
 * 
 * @param board the board array
 * @param size  the size of the board's side
 */
void print_board(const int *board, int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int b = board[j + i * size];
            if (b == 0 && COLOR)
                printf("\033[1;31m");
            printf("%d ", b);
            if (b == 0 && COLOR)
                printf("\033[0m");
        }
        printf("\n");
    }
}


/**
 * Check if the board is a solution. The correct solution is:
 *
 *      1 2 3
 *      4 5 6
 *      7 8 0
 * 
 * @param  board the board array to check
 * @return       1 if the solution is correct, 0 otherwise
 */
int check_solved(const int *board, int size) {
    if (board[size - 1] != 0)
        return 0;

    for (int i = 0; i <= size - 2; ++i)
        if (board[i] != i + 1)
            return 0;

    return 1;
}

/**
 * Create a copy of an array and swap two of its elements 
 * @param origin pointer to the original array
 * @param dest   pointer to the destination array
 * @param size   size of the array
 * @param from   first element to swap
 * @param to     second element to swap
 */
void swap(const int *origin, int *dest, int size, int from, int to) {
    memcpy(dest, origin, size * sizeof(int));
    int temp = dest[to];
    dest[to] = dest[from];
    dest[from] = temp;
}


/**
 * Shuffle an array randomly with a 0 at (0,0)
 * @param array a pointer to the board to shuffle
 * @param n     the size of the array
 */
void shuffle(int *array, size_t n) {
    srand(time(NULL));
    if (n > 1) {
        size_t i;
        for (i = 1; i < n - 1; i++) {
            size_t j = rand() % (i + 1);
            if (j == 0)
                j = 1;
            int temp = array[j];
            array[j] = array[i];
            array[i] = temp;
        }
    }

}

/**
 * Sum the Manhattan distances for each cell of the board while swapping the cell 0 with the cell direction
 * @param board     pointer to the original array
 * @param length    size of the array
 * @param side      size of one side of the 2d array
 * @param direction element to swap with the cell 0
 * @return          Manhattan score
 */
size_t manhattan_distance(const int board[], const size_t length, const size_t side, int direction) {
    size_t total = 0;
    size_t i;
    for (i = 0; i < length; i++) {
        // Compute where the element should be
        int x = (board[i] - 1) % side;
        int y = (board[i] - 1) / side;

        // Element 0 goes at the end
        if(board[i] == 0) {
            x = BOARD_SIDE - 1;
            y = BOARD_SIDE - 1;
        }

        // Compute where the element is
        int x2 = i % side;
        int y2 = i / side;

        total += abs(x - x2) + abs(y - y2);
    }
    return total;
}

/**
 * Compare the Manhattan scores of two moves
 * @param a    move 1
 * @param b    move 2
 * @return to  comparison between the two scores
 */
int cmp_manhattan_distances(const void *a, const void *b) {
    return ((move *) a)->manhattan_distance - ((move *) b)->manhattan_distance;
}

/**
 * Solve the 8-puzzle using the recursive depth-first traversal
 * 
 * @param  board the board array
 * @param  depth the current depth of the recursion
 * @return       a boolean indicating if a solution is found (1 for found, 0 otherwise)
 */
void solve_dfs(int board[], hashmap *hm, int* path, int depth) {
    #pragma omp atomic update
    iter++;

    if (iter % 1000 == 0 && DEBUG)
        printf("current depth: %d, best depth: %d, iterations: %ld, thread: %d\n", depth, best_depth, iter, omp_get_thread_num());

    if (depth >= max_depth || depth >= best_depth)
        return;

    if (check_solved(board, BOARD_LENGTH)) {

        #pragma omp critical
        {
            if(best_depth > depth - 1){
                best_depth = depth - 1;
                memcpy(best_moves, path, best_depth * sizeof(int));
            }
        }

        return;

        if(DEBUG)printf("solved! Best depth: %d, iterations: %ld, thread: %d\n", best_depth, iter, omp_get_thread_num());
            //print_board(board, BOARD_SIDE);
    }


    // calculate position of the 0 (empty cell)
    int pos;
    for (pos = 0; pos < BOARD_LENGTH; pos++) if (board[pos] == 0) break;

    // compute the different moves, -1 if not possible
    move directions[4];
    directions[0].pos = pos % BOARD_SIDE == 0 ? -1 : pos - 1;
    directions[0].dir = LEFT;
    directions[1].pos = pos % BOARD_SIDE == BOARD_SIDE - 1 ? -1 : pos + 1;
    directions[1].dir = RIGHT;
    directions[2].pos = pos - BOARD_SIDE;
    directions[2].dir = UP;
    directions[3].pos = pos + BOARD_SIDE;
    directions[3].dir = DOWN;

    // compute Manhattan distances
    for (int i = 0; i < 4; i++) {
        board[pos] = board[directions[i].pos];
        board[directions[i].pos] = 0;
        directions[i].manhattan_distance =
                directions[i].pos > 0 && directions[i].pos < BOARD_LENGTH ? manhattan_distance(board,
                                                                                                      BOARD_LENGTH,
                                                                                                      BOARD_SIDE,
                                                                                                      directions[i].pos)
                                                                          : INT_MAX;
        board[directions[i].pos] = board[pos];
        board[pos] = 0;
    }

    // sort by manhattan distance
    qsort(directions, 4, sizeof(move), cmp_manhattan_distances);

    #pragma omp taskloop shared(hm) firstprivate(board) if(omp_get_level() < MAX_LEVEL)
    for (int i = 0; i < 4; i++) {
        int direction = directions[i].pos;
        if (direction >= 0 && direction < BOARD_LENGTH) {
            // Create new board and swap the 0 with a possible index
            int *new_board = malloc(BOARD_LENGTH * sizeof(int));
            swap(board, new_board, BOARD_LENGTH, pos, direction);

            // Create a new path with the new index
            int* new_path = malloc((depth + 1) * sizeof(int));
            memcpy(new_path, path, depth *sizeof(int));
            new_path[depth] = directions[i].dir;

            if(hashmap_insert(hm, new_board, depth))
            {
                solve_dfs(new_board, hm, new_path, depth + 1);
            }

            free(new_board);
            free(new_path);
        }
    }
}


/**
 * Parse a string of numbers into a usable grid.
 * @param board  the array to fill
 * @param string the string to convertinto a board
 */
void parse_board(int *board, const char *string) {
    int length = strlen(string);
    int i;

    for (i = 0; i < length; i++) {
        board[i] = string[i] - '0';
    }
}

/**
 * Print a .csv formatted string of the results of the algorithm
 * 
 * @param time_taken   the execution time
 * @param thread_count the number of threads
 */
void print_csv_report(double time_taken, int thread_count)
{
    printf("%d;%d;%d;%ld;%f;%d\n", thread_count, best_depth, max_depth, iter, time_taken, best_depth == INT_MAX);
}

/**
 * Print a nice output of the results of the algorithm
 * 
 * @param time_taken   the execution time
 * @param thread_count the number of threads
 */
void print_text_report(double time_taken, int thread_count)
{
    printf("==================================\n");
    if (best_depth != INT_MAX) {
        printf("Solution found!\nthreads:\t%d\nbest depth:\t%d\nmax depth:\t%d\niterations:\t%ld\ntime(sec):\t%f\npath:\n\n",
            thread_count, best_depth, max_depth, iter, time_taken);
        char *dirs[] = {"UP", "DOWN", "LEFT", "RIGHT"};
        for(int i = 0; i < best_depth + 1; i++){
            printf("  %3d. %s\n",i, dirs[best_moves[i]]);
        }
    } else {
        printf("No solution found.\nthreads:\t%d\nmax depth:\t%d\niterations:\t%ld\ntime(sec):\t%f",
            thread_count, max_depth, iter, time_taken);
    }
}

/**
 * The program's entry point
 * @param  argc arguments count
 * @param  argv arguments value
 * @return      the return code of the program
 */
int main(int argc, char const *argv[]) {
    best_depth = INT_MAX;

    // parsing the max depth argument
    if (argc >= 2)
        max_depth = atoi(argv[1]);
    else
        max_depth = MAX_DEPTH;

    // parsing the board argument
    int board[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    if (argc >= 3) {
        if (strlen(argv[2]) != 9) {
            printf("The given board is incorrect!\n");
            exit(-1);
        }
        parse_board(board, argv[2]);
    } else {
        shuffle(board, (size_t) BOARD_LENGTH);
    }

    int thread_count = omp_get_num_procs();
    
    // num of thread
    if (argc >= 4) 
        thread_count = atoi(argv[3]);

    int output = 0;
    
    // type of output: 0 for text, 1 for csv
    if (argc >= 5) 
        output = atoi(argv[4]);


    hashmap *h = hashmap_create();
    best_moves = malloc(max_depth * sizeof(int));

    if(!output) print_board(board, BOARD_SIDE);

    long t = clock();
    
    #pragma omp parallel num_threads(thread_count) firstprivate(board) shared(h)
    {
        #pragma omp single nowait
        solve_dfs(board, h, NULL, 0);        // start the search
    }
    
    t = clock() - t;
    double time_taken = ((double) t) / CLOCKS_PER_SEC; // in seconds

    if (output)
    {
        print_csv_report(time_taken, thread_count);
    }
    else
    {
        print_text_report(time_taken, thread_count);
    }

    free(best_moves);
    hashmap_free(h);
    return 0;
}