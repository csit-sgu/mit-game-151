#include "internal.hpp"
#include "user.hpp"
#include <raylib.h>

#include <sstream>
#include <fstream>
#include <algorithm>

Vector2 local_to_screen(Context *ctx, Vector2 point)
{
    Vector2 screen_units = ctx->screen_size / PIXEL_PER_UNIT;
    Vector2 d = point - ctx->camera_pos;
    return Vector2 {
        (d.x + screen_units.x * 0.5f) / screen_units.x * ctx->screen_size.x,
        (screen_units.y * 0.5f - d.y) / screen_units.y * ctx->screen_size.y,
    };
}

void Spawn(Context &ctx, Object obj)
{
    ApplyOnSpawn(ctx, obj);
    ctx.to_spawn.push_back(std::move(obj));
}

void Destroy(Context &ctx, Object &obj)
{
    ctx.to_destroy.push_back(obj.id);
}

void UpdateSceneState(Context &ctx)
{
    switch (ctx.state) {
        case GameState::IS_DEAD:
            DrawDeathScreen(ctx);
            break;
        case GameState::FINISHED:
            DrawFinishScreen(ctx);
            break;
        case GameState::GAME_OVER:
            DrawGameOverScreen(ctx);
            break;
        default:
            break;
    }
}

void UpdateGameState(Context &ctx)
{
    switch (ctx.state)
    {
        case GameState::IS_ALIVE: {
            ctx.input_blocked = false;
            Object& player = *find_player(ctx.current_scene);
            if (CheckPlayerDeath(player, ctx.current_scene))
            {
                ctx.state = GameState::IS_DEAD;
            }
            if (CheckFinish(player, ctx.current_scene))
            {
                ctx.state = GameState::FINISHED;
            }
            break;
        }
        case GameState::IS_DEAD: {
            ctx.input_blocked = true;
            if (ctx.lives > 0)
            {
                if (IsKeyPressed(KEY_R))
                {
                    ctx.lives -= 1;
                    ctx.state = GameState::IS_ALIVE;
                    ctx.current_scene = ctx.scenes["game"];
                }
            }
            else
            {
                Object& player = *find_player(ctx.current_scene);
                ctx.state = GameState::GAME_OVER;
                ApplyOnDeath(ctx, player);
            }
            break;
        }
        case GameState::GAME_OVER: {
            ctx.input_blocked = true;
            if (IsKeyPressed(KEY_ENTER))
            {
                ctx.state = GameState::MAIN_MENU;
                ctx.current_scene = ctx.scenes["menu"];
            }
            break;
        }
        case GameState::FINISHED: {
            ctx.input_blocked = true;
            if (IsKeyPressed(KEY_ENTER))
            {
                ctx.state = GameState::MAIN_MENU;
                ctx.current_scene = ctx.scenes["menu"];
            }
            break;
        }
        case GameState::MAIN_MENU: {
            ctx.input_blocked = true;
            ctx.camera_pos = { 0, 0 };
            if (IsKeyPressed(KEY_ENTER))
            {
                ctx.state = GameState::IS_ALIVE;
                ctx.current_scene = ctx.scenes["game"];
                ctx.lives = 3;
                ctx.score = 0;
                ctx.time = 0;
            }
            break;
        }
    }
}

void CreateObjectFromChar(
    Context &ctx,
    Scene &scene,
    char ch,
    float row,
    float col,
    float scale_factor)
{
    switch (ch)
    {
    case '+':
        {
            Object wall1 = Object();
            wall1.position = Vector2 {col, row} * scale_factor;
            wall1.render = Render(
                ctx, "Assets/wall1.png",
                Vector2 {PIXEL_PER_UNIT, PIXEL_PER_UNIT} * scale_factor
            );
            wall1.collider = Collider(wall1.render);
            scene.push_back(std::move(wall1));
        } break;
    case '=':
        {
            Object wall1 = Object();
            wall1.position = Vector2 {col, row} * scale_factor;
            wall1.render = Render(
                ctx, "Assets/wall2.png",
                Vector2 {PIXEL_PER_UNIT, PIXEL_PER_UNIT} * scale_factor
            );
            wall1.collider = Collider(wall1.render);
            scene.push_back(std::move(wall1));
        } break;
    case '*':
        {
            Object wall1 = Object();
            wall1.position = Vector2 {col, row} * scale_factor;
            wall1.render = Render(
                ctx, "Assets/wall3.png",
                Vector2 {PIXEL_PER_UNIT, PIXEL_PER_UNIT} * scale_factor
            );
            wall1.collider = Collider(wall1.render);
            scene.push_back(std::move(wall1));
        } break;
    case 'p':
        {
            Object player = Object();
            player.position = Vector2 {col, row} * scale_factor;
            player.player = Player(10);
            player.render = Render(
                ctx, "Assets/player.png",
                Vector2{ PIXEL_PER_UNIT, PIXEL_PER_UNIT } * scale_factor
            );
            player.collider = Collider(player.render, {ColliderType::DYNAMIC});
            player.physics.enabled = true;
            player.physics.mass = 60;
            scene.push_back(std::move(player));
        } break;
    case '1':
        {
            Object enemy_t1 = Object();
            enemy_t1.position = Vector2 {col, row} * scale_factor;
            enemy_t1.enemy.enabled = true;
            enemy_t1.enemy.speed = 2;
            enemy_t1.render = Render(ctx, "Assets/enemy1.png");
            enemy_t1.collider = Collider(enemy_t1.render, {ColliderType::DYNAMIC});
            enemy_t1.physics.enabled = true;
            enemy_t1.physics.mass = 20;
            scene.push_back(std::move(enemy_t1));
        } break;
    case 'f':
        {
            const float castle_scale = 3.0f;
            Object finish = Object();
            finish.position = Vector2 {col, row} * scale_factor;
            finish.position.y += castle_scale / 2.0f;
            finish.render = Render(
                ctx, "Assets/finish.png",
                Vector2 {
                    PIXEL_PER_UNIT,
                    PIXEL_PER_UNIT
                } * scale_factor * castle_scale
            );
            finish.collider = Collider(finish.render, {ColliderType::EVENT});
            finish.finish.enabled = true;
            scene.push_back(std::move(finish));
        } break;
    case ' ':
        break;
    default:
        std::cerr << "Неизвестный тип объекта" << std::endl;
        break;
    }
}

void ReadScene(Context &ctx, Scene &game_scene, std::string path)
{
    std::ifstream scene_file(path);
    std::stringstream ss;
    ss << scene_file.rdbuf();
    std::string scene_data(ss.str());

    std::vector<std::vector<char>> lvl;
    for (int j = 0; j < int(scene_data.size()); ++j)
    {
        std::vector<char> line;
        while (j < int(scene_data.size()) && scene_data[j] != '\n')
        {
            line.push_back(scene_data[j]);
            j += 1;
        }
        lvl.push_back(line);
    }

    std::reverse(lvl.begin(), lvl.end());

    int lvl_height = int(lvl.size());
    int lvl_width = 0;
    for (const auto &line : lvl)
        lvl_width = std::max(lvl_width, int(line.size()));

    const float scale_factor = 1.6f;
    Vector2 lvl_size = Vector2 {
        float(lvl_width),
        float(lvl_height)
    } * scale_factor;

    Object bg0 = Object();
    bg0.position = lvl_size * 0.5f - Vector2 { 0.5f, 0.5f } * scale_factor;
    bg0.render = Render(ctx, "Assets/background.png", lvl_size * PIXEL_PER_UNIT);
    game_scene.push_back(bg0);
    
    for (int row = 0; row < int(lvl.size()); ++row)
    {
        for (int col = 0; col < int(lvl[row].size()); ++col)
        {
            const char ch = lvl[row][col];
            CreateObjectFromChar(ctx, game_scene, lvl[row][col], row, col, scale_factor);
        }
    }
}

bool operator==(const Object& lhs, const Object& rhs)
{
    return (lhs.id == rhs.id);
}

bool operator!=(const Object& lhs, const Object& rhs)
{
    return !(lhs == rhs);
}

Object* find_player(Scene &scene)
{
    for (auto &obj : scene)
        if (obj.player.enabled)
            return &obj;
    return nullptr;
}

