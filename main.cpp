#include "raylib.h"

#include <algorithm>
#include <array>
#include <deque>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

constexpr int COLS = 25; // Odd dimensions allow corridor carving
constexpr int ROWS = 19;
constexpr int CELL = 29;
constexpr int LEFT = 24;
constexpr int TOP = 142;
constexpr int WIDTH = LEFT * 2 + COLS * CELL;
constexpr int HEIGHT = TOP + ROWS * CELL + 95;

using Point = pair<int, int>; // (row, col)
const Point START = {1, 1};
const Point GOAL = {ROWS - 2, COLS - 2};
const array<Point, 4> DIRS = {{{-1, 0}, {0, 1}, {1, 0}, {0, -1}}};

struct MazeApp {
    vector<vector<int>> wall;
    vector<vector<bool>> seen;
    vector<vector<bool>> explored;
    vector<vector<bool>> onPath;
    vector<vector<Point>> parent;
    deque<Point> frontier;
    mt19937 rng{random_device{}()};

    bool bfs = true;
    bool paused = true;
    bool finished = false;
    bool found = false;
    int exploredCount = 0;
    int pathLength = 0;
    int stepsPerSecond = 12;
    double elapsed = 0.0;
    Point current = START;

    void generateMaze() {
        wall.assign(ROWS, vector<int>(COLS, 1));
        vector<Point> stack;
        stack.push_back(START);
        wall[START.first][START.second] = 0;

        // Recursive-backtracker maze generation, implemented iteratively.
        while (!stack.empty()) {
            Point p = stack.back();
            vector<Point> choices;
            for (auto [dr, dc] : DIRS) {
                int nr = p.first + dr * 2;
                int nc = p.second + dc * 2;
                if (nr > 0 && nr < ROWS - 1 && nc > 0 && nc < COLS - 1 && wall[nr][nc]) {
                    choices.push_back({nr, nc});
                }
            }
            if (choices.empty()) {
                stack.pop_back();
                continue;
            }
            uniform_int_distribution<int> pick(0, static_cast<int>(choices.size()) - 1);
            Point next = choices[pick(rng)];
            wall[(p.first + next.first) / 2][(p.second + next.second) / 2] = 0;
            wall[next.first][next.second] = 0;
            stack.push_back(next);
        }
        // Add a few loops, so BFS and DFS can choose different routes.
        uniform_int_distribution<int> chance(0, 99);
        for (int r = 1; r < ROWS - 1; ++r) {
            for (int c = 1; c < COLS - 1; ++c) {
                if (!wall[r][c] || (r % 2) == (c % 2)) continue;
                bool horizontal = !wall[r][c - 1] && !wall[r][c + 1];
                bool vertical = !wall[r - 1][c] && !wall[r + 1][c];
                if ((horizontal || vertical) && chance(rng) < 18) wall[r][c] = 0;
            }
        }
        resetSearch();
    }

    void resetSearch() {
        seen.assign(ROWS, vector<bool>(COLS, false));
        explored.assign(ROWS, vector<bool>(COLS, false));
        onPath.assign(ROWS, vector<bool>(COLS, false));
        parent.assign(ROWS, vector<Point>(COLS, {-1, -1}));
        frontier.clear();
        frontier.push_back(START);
        seen[START.first][START.second] = true;
        current = START;
        exploredCount = 0;
        pathLength = 0;
        elapsed = 0;
        paused = true;
        finished = false;
        found = false;
    }

    void tracePath() {
        Point p = GOAL;
        while (p != START) {
            onPath[p.first][p.second] = true;
            p = parent[p.first][p.second];
            ++pathLength;
        }
        onPath[START.first][START.second] = true;
    }

    void step() {
        if (finished || frontier.empty()) {
            finished = true;
            paused = true;
            return;
        }

        Point p;
        if (bfs) {
            p = frontier.front(); // Queue: FIFO
            frontier.pop_front();
        } else {
            p = frontier.back();  // Stack: LIFO
            frontier.pop_back();
        }
        current = p;
        explored[p.first][p.second] = true;
        ++exploredCount;

        if (p == GOAL) {
            found = true;
            finished = true;
            paused = true;
            tracePath();
            return;
        }

        for (auto [dr, dc] : DIRS) {
            int nr = p.first + dr;
            int nc = p.second + dc;
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (wall[nr][nc] || seen[nr][nc]) continue;
            seen[nr][nc] = true; // Mark when inserted, not removed
            parent[nr][nc] = p;
            frontier.push_back({nr, nc});
        }
    }

    void update() {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_TWO)) {
            bfs = IsKeyPressed(KEY_ONE);
            resetSearch();
        }
        if (IsKeyPressed(KEY_R)) resetSearch();
        if (IsKeyPressed(KEY_N)) generateMaze();
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_EQUAL))
            stepsPerSecond = min(120, stepsPerSecond + 5);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_MINUS))
            stepsPerSecond = max(1, stepsPerSecond - 5);
        if (IsKeyPressed(KEY_SPACE) && !finished) paused = !paused;
        if (IsKeyPressed(KEY_RIGHT) && paused && !finished) step();

        if (!paused && !finished) {
            elapsed += GetFrameTime();
            // Limit iterations per frame; avoids runaway work on low FPS.
            int iterations = 0;
            while (elapsed >= 1.0 / stepsPerSecond && iterations < 50 && !finished) {
                step();
                elapsed -= 1.0 / stepsPerSecond;
                ++iterations;
            }
        }
    }

    void draw() const {
        const Color bg = {13, 19, 31, 255};
        const Color wallColor = {34, 43, 59, 255};
        const Color openColor = {239, 244, 250, 255};
        const Color queuedColor = {169, 216, 245, 255};
        const Color exploredColor = {91, 163, 221, 255};
        const Color routeColor = {247, 197, 77, 255};
        const Color startColor = {44, 194, 135, 255};
        const Color goalColor = {238, 92, 109, 255};

        ClearBackground(bg);
        DrawText("MAZE SEARCH VISUALIZER", LEFT, 19, 28, RAYWHITE);
        DrawText(bfs ? "MODE: BFS (queue)" : "MODE: DFS (stack)", LEFT, 59, 21, {128, 207, 255, 255});
        DrawText(TextFormat("Visited: %d   Path: %d steps   Speed: %d/s", exploredCount,
                            pathLength, stepsPerSecond), LEFT, 91, 18, LIGHTGRAY);

        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                Color color = openColor;
                if (wall[r][c]) color = wallColor;
                else if (seen[r][c]) color = queuedColor;
                if (explored[r][c]) color = exploredColor;
                if (onPath[r][c]) color = routeColor;
                if (Point{r, c} == START) color = startColor;
                if (Point{r, c} == GOAL) color = goalColor;
                int x = LEFT + c * CELL;
                int y = TOP + r * CELL;
                DrawRectangle(x + 1, y + 1, CELL - 2, CELL - 2, color);
            }
        }
        if (exploredCount > 0 && !finished) {
            int x = LEFT + current.second * CELL + CELL / 2;
            int y = TOP + current.first * CELL + CELL / 2;
            DrawCircleLines(x, y, CELL / 3.0f, BLACK);
        }

        int footer = TOP + ROWS * CELL + 17;
        DrawText("1 BFS  2 DFS  SPACE Play/Pause  RIGHT Step", LEFT, footer, 17, RAYWHITE);
        DrawText("N New maze  R Restart  UP/DOWN Speed", LEFT, footer + 27, 17, LIGHTGRAY);
        if (finished) {
            DrawText(found ? "GOAL FOUND!" : "NO PATH", WIDTH - 180, footer + 27, 18,
                     found ? startColor : goalColor);
        } else if (paused) {
            DrawText("PAUSED", WIDTH - 110, footer + 27, 18, routeColor);
        }
    }
};

int main() {
    InitWindow(WIDTH, HEIGHT, "BFS vs DFS - Maze Visualizer");
    SetTargetFPS(60);
    MazeApp app;
    app.generateMaze();

    while (!WindowShouldClose()) {
        app.update();
        BeginDrawing();
        app.draw();
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
