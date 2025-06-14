#include <ncurses.h>
#include <vector>
#include <deque>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <string>


const int EMPTY_SPACE = 0;
const int WALL = 1;
const int IMMUNE_WALL = 2;
const int SNAKE_HEAD = 3;
const int SNAKE_BODY = 4;
const int GROWTH_ITEM = 5;
const int POISON_ITEM = 6;
const int GATE = 7;

enum Direction { UP, DOWN, LEFT, RIGHT };

struct Point {
    int y, x;
    bool operator==(const Point& other) const { return y == other.y && x == other.x; }
};

struct Item { Point pos; int type; time_t spawn_time; };
struct Mission { int length, growth, poison, gate; bool completed = false; };



class GameMap {
public:
    std::vector<std::vector<int>> map_data;
    int height, width;

    GameMap(int h = 25, int w = 52);
    void createMapForStage(int stage);
    void draw(WINDOW* win);
};

class Snake {
public:
    Direction dir;
    std::deque<Point> body;
    int max_len, growth_count, poison_count, gate_count;

    Snake();
    void reset(int y, int x);
    void changeDir(int key);
    void grow();
    void shrink();
    int len();
    int getMaxLen();
    int getGrowthCount();
    int getPoisonCount();
    int getGateCount();
};

class Game {
public:
    Game();
    ~Game();
    void run();

private:
    void setup_ncurses();
    void handleInput();
    void loadStage(int s);
    void createGates();
    void update();
    void checkMission();
    void render();
    void showGameOver();

    WINDOW *win_game, *win_score, *win_mission;
    GameMap map;
    Snake snake;
    std::vector<Item> items;
    Point gate1, gate2;
    time_t last_gate_time;
    time_t stage_start_time;
    int stage;
    Mission current_mission;
    bool game_over;
    std::vector<Point> wall_locations;
};





GameMap::GameMap(int h, int w) : height(h), width(w) {}

void GameMap::createMapForStage(int stage) {
    map_data.assign(height, std::vector<int>(width, 0));
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            if (i == 0 || i == height - 1 || j == 0 || j == width - 1) {
                 if ((i==0 && j==0) || (i==0 && j==width-1) || (i==height-1 && j==0) || (i==height-1 && j==width-1))
                    map_data[i][j] = IMMUNE_WALL;
                 else
                    map_data[i][j] = WALL;
            }
        }
    }
    if (stage == 2 || stage == 4) { for(int i = 5; i < height - 5; ++i) map_data[i][15] = WALL; }
    if (stage == 3 || stage == 4) { for(int j = 15; j < width - 15; ++j) map_data[12][j] = WALL; }
}

void GameMap::draw(WINDOW* win) {
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            switch (map_data[i][j]) {
                case WALL: mvwprintw(win, i, j, "#"); break;
                case IMMUNE_WALL: mvwprintw(win, i, j, "X"); break;
                case SNAKE_HEAD: mvwprintw(win, i, j, "H"); break;
                case SNAKE_BODY: mvwprintw(win, i, j, "o"); break;
                case GROWTH_ITEM: mvwprintw(win, i, j, "+"); break;
                case POISON_ITEM: mvwprintw(win, i, j, "-"); break;
                case GATE: mvwprintw(win, i, j, "G"); break;
                default: mvwprintw(win, i, j, " "); break;
            }
        }
    }
}


Snake::Snake() { reset(12, 25); }

void Snake::reset(int y, int x) {
    body.clear();
    dir = RIGHT;
    body.push_back({y, x});
    body.push_back({y, x - 1});
    body.push_back({y, x - 2});
    
    max_len = 3;
    growth_count = 0;
    poison_count = 0;
    gate_count = 0;
}

void Snake::changeDir(int key) {
    if (key == KEY_UP && dir != DOWN) dir = UP;
    else if (key == KEY_DOWN && dir != UP) dir = DOWN;
    else if (key == KEY_LEFT && dir != RIGHT) dir = LEFT;
    else if (key == KEY_RIGHT && dir != LEFT) dir = RIGHT;
}

void Snake::grow() {
    growth_count++;
    if (len() + 1 > max_len) max_len = len() + 1;
}

void Snake::shrink() {
    poison_count++;
    if (body.size() > 0) body.pop_back();
}

int Snake::len() { return body.size(); }
int Snake::getMaxLen() { return max_len; }
int Snake::getGrowthCount() { return growth_count; }
int Snake::getPoisonCount() { return poison_count; }
int Snake::getGateCount() { return gate_count; }

// Game 함수 정의
Game::Game() : stage(1), game_over(false), gate1({0,0}), gate2({0,0}) {
    setup_ncurses();
    srand(time(NULL));
    loadStage(stage);
}

Game::~Game() {
    delwin(win_game);
    delwin(win_score);
    delwin(win_mission);
    endwin();
}

void Game::run() {
    while (!game_over) {
        handleInput();
        update();
        render();
        
        if (snake.dir == LEFT || snake.dir == RIGHT) {
            usleep(100000);
        } else {
            usleep(200000);
        }
    }
    showGameOver();
}

void Game::setup_ncurses() {
    initscr();
    curs_set(0);
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    win_game = newwin(25, 52, 0, 0);
    win_score = newwin(12, 30, 0, 53);
    win_mission = newwin(13, 30, 12, 53);
}

void Game::handleInput() {
    int ch = getch();
    if (ch != ERR) {
        snake.changeDir(ch);
    }
}

void Game::loadStage(int s) {
    if (s > 4) {
        game_over = true;
        return;
    }
    stage_start_time = time(NULL); 
    stage = s;
    map.createMapForStage(s);
    snake.reset(12, 25);
    items.clear();
    wall_locations.clear();

    for(int y = 0; y < map.height; ++y) {
        for(int x = 0; x < map.width; ++x) {
            if(map.map_data[y][x] == WALL) wall_locations.push_back({y,x});
        }
    }
    createGates();
    
    switch(s) {
        case 1: current_mission = {5, 2, 1, 1}; break;
        case 2: current_mission = {7, 4, 2, 2}; break;
        case 3: current_mission = {10, 6, 3, 3}; break;
        case 4: current_mission = {15, 8, 4, 4}; break;
    }
    current_mission.completed = false;
}

void Game::createGates() {
     if (wall_locations.size() < 2) return;
    if (map.map_data[gate1.y][gate1.x] == GATE) map.map_data[gate1.y][gate1.x] = WALL;
    if (map.map_data[gate2.y][gate2.x] == GATE) map.map_data[gate2.y][gate2.x] = WALL;

    int idx1 = rand() % wall_locations.size();
    int idx2;
    do { idx2 = rand() % wall_locations.size(); } while (idx1 == idx2);
    gate1 = wall_locations[idx1];
    gate2 = wall_locations[idx2];
    map.map_data[gate1.y][gate1.x] = GATE;
    map.map_data[gate2.y][gate2.x] = GATE;
    last_gate_time = time(NULL);
}

void Game::update() {
    if (time(NULL) - last_gate_time > 10) createGates();
    
    time_t now = time(NULL);
    for(auto it = items.begin(); it != items.end(); ) {
        if(now - it->spawn_time > 5) {
            map.map_data[it->pos.y][it->pos.x] = EMPTY_SPACE;
            it = items.erase(it);
        } else ++it;
    }
    while(items.size() < 3) {
        Point p;
        do { p = {rand() % map.height, rand() % map.width}; } while(map.map_data[p.y][p.x] != EMPTY_SPACE);
        int type = (rand() % 2 == 0) ? GROWTH_ITEM : POISON_ITEM;
        items.push_back({{p.y, p.x}, type, time(NULL)});
        map.map_data[p.y][p.x] = type;
    }

    Point head = snake.body.front();
    Point next_head = head;
    switch(snake.dir) {
        case UP: next_head.y--; break;
        case DOWN: next_head.y++; break;
        case LEFT: next_head.x--; break;
        case RIGHT: next_head.x++; break;
    }
    
    if (map.map_data[next_head.y][next_head.x] == GATE) {
         snake.gate_count++;
         Point entry_gate = next_head;
         Point exit_gate = (entry_gate == gate1) ? gate2 : gate1;

         if (exit_gate.y == 0) snake.dir = DOWN;
         else if (exit_gate.y == map.height - 1) snake.dir = UP;
         else if (exit_gate.x == 0) snake.dir = RIGHT;
         else if (exit_gate.x == map.width - 1) snake.dir = LEFT;

         switch(snake.dir) {
            case UP: next_head = {exit_gate.y-1, exit_gate.x}; break;
            case DOWN: next_head = {exit_gate.y+1, exit_gate.x}; break;
            case LEFT: next_head = {exit_gate.y, exit_gate.x-1}; break;
            case RIGHT: next_head = {exit_gate.y, exit_gate.x+1}; break;
         }
    }

    for(const auto& part : snake.body) { if(next_head == part) { game_over = true; return; }}
    int next_pos_val = map.map_data[next_head.y][next_head.x];
    if(next_pos_val == WALL || next_pos_val == IMMUNE_WALL) { game_over = true; return; }

    bool ate_growth = false;
    if(next_pos_val == GROWTH_ITEM) { snake.grow(); ate_growth = true; }
    else if (next_pos_val == POISON_ITEM) { snake.shrink(); }
    
    if (next_pos_val == GROWTH_ITEM || next_pos_val == POISON_ITEM) {
        for(auto it = items.begin(); it != items.end(); ++it) {
            if(it->pos == next_head) { items.erase(it); break; }
        }
    }
    
    snake.body.push_front(next_head);
    if (!ate_growth) {
        snake.body.pop_back();
    }
    
    if (snake.len() < 3) { game_over = true; return; }

    for(auto& row : map.map_data) {
        for(auto& cell : row) {
            if(cell == SNAKE_HEAD || cell == SNAKE_BODY) cell = EMPTY_SPACE;
        }
    }
    for(size_t i = 0; i < snake.body.size(); ++i) {
        map.map_data[snake.body[i].y][snake.body[i].x] = (i==0) ? SNAKE_HEAD : SNAKE_BODY;
    }

    checkMission();
}

void Game::checkMission() {
    bool len_ok = snake.getMaxLen() >= current_mission.length;
    bool growth_ok = snake.getGrowthCount() >= current_mission.growth;
    bool poison_ok = snake.getPoisonCount() >= current_mission.poison;
    bool gate_ok = snake.getGateCount() >= current_mission.gate;
    
    if (len_ok && growth_ok && poison_ok && gate_ok) {
        current_mission.completed = true;
        loadStage(stage + 1);
    }
}

void Game::render() {
    werase(stdscr); werase(win_game); werase(win_score); werase(win_mission);
    
    map.draw(win_game);
    
    box(win_score, 0, 0);
    mvwprintw(win_score, 1, 2, "SCORE BOARD");
    mvwprintw(win_score, 3, 2, "B: %d / %d", snake.len(), snake.getMaxLen());
    mvwprintw(win_score, 4, 2, "+: %d", snake.getGrowthCount());
    mvwprintw(win_score, 5, 2, "-: %d", snake.getPoisonCount());
    mvwprintw(win_score, 6, 2, "G: %d", snake.getGateCount());
    
    
    time_t current_time = time(NULL);
    int elapsed_seconds = current_time - stage_start_time;
    mvwprintw(win_score, 7, 2, "Time: %d s", elapsed_seconds);

    mvwprintw(win_score, 9, 2, "Stage: %d", stage); 
    
    box(win_mission, 0, 0);
    mvwprintw(win_mission, 1, 2, "MISSION");
    mvwprintw(win_mission, 3, 2, "B: %d (%c)", current_mission.length, snake.getMaxLen() >= current_mission.length ? 'v' : ' ');
    mvwprintw(win_mission, 4, 2, "+: %d (%c)", current_mission.growth, snake.getGrowthCount() >= current_mission.growth ? 'v' : ' ');
    mvwprintw(win_mission, 5, 2, "-: %d (%c)", current_mission.poison, snake.getPoisonCount() >= current_mission.poison ? 'v' : ' ');
    mvwprintw(win_mission, 6, 2, "G: %d (%c)", current_mission.gate, snake.getGateCount() >= current_mission.gate ? 'v' : ' ');

    refresh(); wrefresh(win_game); wrefresh(win_score); wrefresh(win_mission);
}

void Game::showGameOver() {
    nodelay(stdscr, FALSE);
    werase(win_game);
    box(win_game, 0, 0);
    if (stage > 4) {
        mvwprintw(win_game, 11, 18, "S T A G E C L E A R !");
    } else {
        mvwprintw(win_game, 11, 19, "G A M E O V E R");
    }
    wrefresh(win_game);
    sleep(100);
}


int main() {
    Game game;
    game.run();
    return 0;
}