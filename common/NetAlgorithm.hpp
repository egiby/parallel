#include <iostream>
#include <vector>
#include <chrono>

using std::cerr;

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "board.h"
#include "common.h"

#include "IConnectionClient.hpp"

int * get_line_board(Board * board, int thread_idx, int num_threads)
{
    int size = get_size(board->N, thread_idx, num_threads);
    int l = calc_left(board->N, thread_idx, num_threads);
    int * line_board = (int*)malloc(size * board->M * sizeof(int));
    
    for (int i = 0; i < size; ++i)
        memcpy(line_board + i * board->M, board->board[i + l], board->M * sizeof(int));
    
    return line_board;
}

Board * get_board_from_line(int * line_board, int n, int m)
{
    Board * board = get_board(n, m);
    
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < m; ++j)
            board->board[i][j] = line_board[i * m + j];
    
    return board;
}

int get_line_board_elem(int * board, int * top, int * bottom, int x, int y, int size, int m)
{
    int ans = -1;
    
    if (y < 0)
        y += m;
    
    if (y >= m)
        y -= m;
    
    if (x == -1)
        ans = top[y];
    else if (x == size)
        ans = bottom[y];
    else
        ans = board[x * m + y];
    
    return ans;
}

class Calculator
{
    Calculator()
    {
    }
    
    int * thread_board;
    int * top;
    bool is_top_calculated;
    int * bottom;
    bool is_bottom_calculated;
    int * new_board;
    int m, size, cur_x;
    
    void calculate_line(int x)
    {
        for (int y = 0; y < m; ++y)
        {
            int old_value = get_line_board_elem(thread_board, top, bottom, x, y, size, m);
            int cnt = 0;
            
            for (int l = 0; l < 8; ++l)
                cnt += get_line_board_elem(thread_board, top, bottom, x + dx[l], y + dy[l], size, m);
            
            new_board[x * m + y] = get_life_value(cnt, old_value);
        }
    }
    
public:
    
    Calculator(int * thread_board, int * new_board, int m, int size)
    : thread_board(thread_board), new_board(new_board), m(m), size(size), cur_x(1)
    {
        is_top_calculated = 0;
        is_bottom_calculated = 0;
    }
    
    void calculate_next()
    {
        if (cur_x < size - 1)
            calculate_line(cur_x++);
    }
    
    void set_top(int * new_top)
    {
        top = new_top;
    }
    
    void calculate_top()
    {
        calculate_line(0);
    }
    
    void set_bottom(int * new_bottom)
    {
        bottom = new_bottom;
    }
    
    void calculate_bottom()
    {
        calculate_line(size - 1);
    }
    
    void calculate_all()
    {
        if (!is_top_calculated)
            calculate_top();
        if (!is_bottom_calculated)
            calculate_bottom();
        
        for (; cur_x < size - 1; ++cur_x)
            calculate_line(cur_x);
    }
    
    bool done()
    {
        return cur_x == size - 1 && is_top_calculated && is_bottom_calculated;
    }
};

void play(IConnectionClient * connection, int argc, char ** argv)
{
    connection->init(argc, argv);
    
    int num_threads = connection->get_num_threads();
    
    int thread_idx = connection->get_thread_id();
    
    int *thread_board;
    
    int n, m, k;
    
    int size;
    char is_reversed = 0;
    
    std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();
    
    // reading
    if (thread_idx == 0)
    {
        FILE * file = fopen(argv[1], "r");
        fscanf(file, "%d %d %d\n", &n, &m, &k);
        
        Board * board = get_board(n, m);
        
        read_board(board, file);
        
        if (n < m)
        {
            reverse_board(board);
            swap_int(&n, &m);
            is_reversed = 1;
        }
        
        size = get_size(n, thread_idx, num_threads);
        //~ std::cerr << "send board\n";
        for (int i = 1; i < num_threads; ++i)
        {
            int size_i = get_size(n, i, num_threads);
            int * line_board = get_line_board(board, i, num_threads);
            
            connection->send(&n, 1, i, 50000);
            connection->send(&m, 1, i, 50001);
            connection->send(&k, 1, i, 50002);
            connection->send(&size_i, 1, i, 50003);
            connection->send(line_board, size_i * m, i, 50004);
            
            free(line_board);
        }
        
        thread_board = get_line_board(board, 0, num_threads);
        
        //~ std::cerr << "board sended\n";
        print_board(board);
        
        delete_board(board);
    }
    else
    {
        //~ std::cerr << "recv board\n";
        connection->recv(&n, 1, 0, 50000);
        connection->recv(&m, 1, 0, 50001);
        connection->recv(&k, 1, 0, 50002);
        connection->recv(&size, 1, 0, 50003);
        
        //~ std::cerr << "ints recved\n";
        
        thread_board = (int*)malloc(size * m * sizeof(int));
        connection->recv(thread_board, size * m, 0, 50004);
        //~ std::cerr << "board recved\n";
    }
    
    //~ for (int x = 0; x < size; ++x)
    //~ {
        //~ for (int y = 0; y < m; ++y)
        //~ {
            //~ std::cerr << thread_board[x * m + y];
        //~ }
        //~ std::cerr << '\n';
    //~ }
    
    
    
    //~ std::cerr << "calculating started\n";
    
    int * top = (int*)malloc(m * sizeof(int));
    int * bottom = (int*)malloc(m * sizeof(int));
    
    int * new_board = (int*)malloc(size * m * sizeof(int));
    
    for (int i = 0; i < k; ++i)
    {
        Calculator calc(thread_board, new_board, m, size);
        int left_thread = (thread_idx - 1 + num_threads) % num_threads;
        int right_thread = (thread_idx + 1) % num_threads;
        
        IRequest * left_send = connection->async_send(thread_board, m, left_thread, 2 * i);
        IRequest * right_send = connection->async_send(thread_board + (size - 1) * m, m, right_thread, 2 * i + 1);
        
        IRequest * top_recv = connection->async_recv(top, m, left_thread, 2 * i + 1);
        IRequest * bottom_recv = connection->async_recv(bottom, m, right_thread, 2 * i);
        
        while (!top_recv->test() || ! bottom_recv->test())
        {
            calc.calculate_next();
        }
        calc.set_bottom(bottom);
        calc.set_top(top);
        
        calc.calculate_all();
        
        left_send->wait();
        right_send->wait();
        swap_iters(&new_board, &thread_board);
    }
    
    //~ std::cerr << "calculating ended\n";
    // answering

    if (thread_idx != 0)
    {
        IRequest * request = connection->async_send(thread_board, size * m, 0, thread_idx + 25000);
        
        request->wait();
    }
    else
    {
        int * line_board = NULL;
        int * count_r = NULL;
        int * disp_r = NULL;
    
        line_board = (int*)malloc(sizeof(int) * n * m);
        count_r = (int*)malloc(sizeof(int) * num_threads);
        disp_r = (int*)malloc(sizeof(int) * num_threads);
        
        int last = 0;
        for (int i = 0; i < num_threads; ++i)
        {
            disp_r[i] = last;
            count_r[i] = get_size(n, i, num_threads) * m;
            last += count_r[i];
        }
        
        std::vector<IRequest*> r;
        
        for (int i = 1; i < num_threads; ++i)
        {
            r.push_back(connection->async_recv(line_board + disp_r[i], count_r[i], i, i + 25000));
        }
        
        memcpy(line_board, thread_board, size * m * sizeof(int));
        
        //~ perror("wait all");
        
        for (int i = 0; i < (int)r.size(); ++i)
            r[i]->wait();
        // printing
        
        //~ cerr << "wait all\n";
        
        Board * board = get_board_from_line(line_board, n, m);
        
        if (is_reversed)
            reverse_board(board);
        
        print_board(board);
        
        delete_board(board);
        free(line_board);
        free(count_r);
        free(disp_r);
    }
    
    free(thread_board);
    free(new_board);
    
    if (thread_idx == 0)
    {
        std::chrono::time_point<std::chrono::system_clock> end_time = std::chrono::system_clock::now();
        std::cerr << num_threads << ' ' << std::chrono::duration<double>(end_time - start_time).count() << '\n';
    }
    //~ cerr << "finalized\n";
    connection->finalize();
}
