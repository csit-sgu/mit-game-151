#pragma once

#include <iostream>
#include <memory>
#include <raylib-ext.hpp>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <string>
#include <sstream>
#include <map>

const int PIXEL_PER_UNIT = 30;
const float GRAVITY = 90.0f;
const float MAX_FALLING_SPEED = -200.0f;
const float CAMERA_SPEED = 50.0f;

struct Object;
struct Render;
typedef std::vector<Object> Scene;
typedef size_t GameId;
typedef unsigned long long TextureHash;

enum class GameState
{
    IS_ALIVE,
    IS_DEAD,
    GAME_OVER,
    MAIN_MENU,
    FINISHED
};

// Структура Context, в которой хранятся некоторые переменные текущего состояния
// игры. При реализации своих функций вам понадобятся не все поля, но, я думаю,
// по названиям большинства этих переменных можно понять что в них хранится.
struct Context
{
    Vector2 camera_pos;
    Vector2 screen_size;
    int lives;
    std::unique_ptr<Render> heart;
    int score;
    uint64_t time;
    GameState state;
    bool input_blocked;
    std::map<TextureHash, Texture> textures_storage;
    std::vector<GameId> to_destroy;
    std::vector<Object> to_spawn;
    Scene current_scene;
    std::map<std::string, std::vector<Object>> scenes;
};

// Реализации следующих функций находятся в файле internal.cpp.

// Функция local_to_screen преобразует переданную координату из игровой
// системы координат в систему координат, которая используется при отрисовке.
// Для выполнения своих задания эта функция вам не нужна.
Vector2 local_to_screen(Context *, Vector2 point);

// Функция Destroy получает в качестве аргументов контекст игры и объект,
// который необходимо удалить. Так как во многих функциях происходит
// итерация по списку объектов в сцене, то объект нельзя добавить в сцену сразу
// же при вызове функции. Поэтому она помещает объект в список to_destroy,
// из которого объект удалится в конце отрисовки кадра.
void Destroy(Context &ctx, Object &obj);

// Функция Destroy получает в качестве аргументов контекст игры и объект,
// который необходимо добавить в сцена. Работает аналогично функции Destroy,
// поэтому объект добавится в сцену только в конце текущего кадра, а
// взаимодействовать с ним получится только на следующем кадре.
void Spawn(Context &ctx, Object obj);

// Функция UpdateSceneState просто вызывает функцию отрисовки некоторого
// экрана, соответствующего текущему состоянию игры. Например, если игрок
// пришёл к финишу, вызовется функция DrawFinishScreen, и т.д.
void UpdateSceneState(Context &);

// Функция UpdateGameState переключает состояние поля Context::state в
// зависимости от событий, происходящих в игре. При реализации своих заданий
// эта функция вам не понадобится, поэтому не будем подробнее описывать её
// работу, но реализацию всегда можно просто посмотреть в файле internal.cpp.
void UpdateGameState(Context &);

// Функция find_player находит объект со включённым компонентом Player в
// переданной сцене и возвращает указатель на него. Возвращается именно
// указатель для того, чтобы изменения, которые вносятся в этот объект
// действительно применялись к игроку. Иначе, возвращалась бы копия и
// изменения бы не применялись.
//
// Такая реализация не очень безопасна, так как Scene (которая на самом деле
// std::vector<Object>) может динамически менять свой размер и из-за этого
// указатель может сломаться. Поэтому важно каждый раз вызывать эту функцию
// заново, а не сохранять её результат для дальнейшего использования.
Object* find_player(Scene &);

// Функция ReadScene считывает сцену из файла по переданному пути. Эта функция
// также вызывается только при инициализации игры, поэтому не будем её
// подробно описывать
void ReadScene(Context &, Scene &, std::string path);


// Далее следуют компоненты всех игровых объектов. У большинства из них
// тривиальные конструкторы, поэтому комментарии есть только к тем объектам,
// которые при конструировании делают не только присвоение переменных.


// Структура Render используется для отрисовки всех объектов. У неё есть
// несколько вариантов использования и поэтому реализовано несколько
// конструкторов.
//
// Одной из заметных частей является тот факт, что объекты Render не хранят
// в себе сами текстуры, которые им соответствуют. Вместо этого они хранят
// некоторый хеш текстуры, который вычисляется в каждом из конструкторов.
// Сами же текстуры добавляются в поле Context::textures_storage. Сделано это
// для того, чтобы для разных объектов с одинаковыми текстурами эти самые
// текстуры не загружались по несколько раз. Таким образом, экономится некоторое
// количество оперативной памяти и добавляется немного производительности при
// загрузке текстур.
struct Render
{
    bool visible;
    float width, height;
    std::string path;
    TextureHash hash;

    // Самый простой конструктор. Загружает переданный файл с текстурой и
    // никак его не изменяет.
    Render(Context &ctx, std::string filename) : visible(true)
    {
        hash = CalculateTextureHash(filename, 0, 0);
        if (ctx.textures_storage.count(hash) == 0)
        {
            Texture tex = LoadTexture(filename);
            ctx.textures_storage[hash] = std::move(tex);
        }
        Texture tex = ctx.textures_storage[hash];
        width = float(tex.width);
        height = float(tex.height);
        path = filename;
    }

    // Наиболее используемый конструктор. При загрузке текстуры он также
    // масшатбирует её размер на переданное значение scale. Если scale < 1,
    // то текстура уменьшается, иначе - увеличивается.
    Render(Context &ctx, std::string filename, float scale) : visible(true)
    {
        hash = CalculateTextureHash(filename, scale, 0.0f);
        if (ctx.textures_storage.count(hash) == 0)
        {
            Image img = LoadImage(filename);
            ImageResize(&img, img.width * scale, img.height * scale);
            Texture tex = LoadTextureFromImage(img);
            UnloadImage(img);
            ctx.textures_storage[hash] = std::move(tex);
        }
        Texture tex = ctx.textures_storage[hash];
        path = filename;
        width = tex.width;
        height = tex.height;
    }

    // Данный конструктор при загрузке текстура изменяет её размер в пикселях на
    // указанный в параметре size. В отличие от конструктора со scale, не
    // сохраняет оригинальное соотношение сторон картинки.
    Render(Context &ctx, std::string filename, Vector2 size) : visible(true)
    {
        hash = CalculateTextureHash(filename, size.x, size.y);
        if (ctx.textures_storage.count(hash) == 0)
        {
            Image img = LoadImage(filename);
            ImageResize(&img, size.x, size.y);
            Texture tex = LoadTextureFromImage(img);
            UnloadImage(img);
            ctx.textures_storage[hash] = std::move(tex);
        }
        Texture tex = ctx.textures_storage[hash];
        path = filename;
        width = size.x;
        height = size.y;
    }
    Render() : visible(false) {}

private:
    // Вычисляет хэш по переданным параметрам. Для вычисления используется
    // очень простой алгоритм Adler32.
    TextureHash CalculateTextureHash(std::string path, float width, float height)
    {
        const char *path_bytes = path.c_str();
        char *width_bytes = reinterpret_cast<char *>(&width);
        char *height_bytes = reinterpret_cast<char *>(&height);

        uint64_t mod64 = 4294967291;
        size_t path_len = strlen(path_bytes);
        uint64_t s1 = 1;
        uint64_t s2 = 0;

        for (int i = 0; i < path_len; ++i)
        {
            s1 = (s1 + path_bytes[i]) % mod64;
            s2 = (s2 + s1) % mod64;
        }

        for (int i = 0; i < 4; ++i)
        {
            s1 = (s1 + width_bytes[i]) % mod64;
            s2 = (s2 + s1) % mod64;
        }

        for (int i = 0; i < 4; ++i)
        {
            s1 = (s1 + height_bytes[i]) % mod64;
            s2 = (s2 + s1) % mod64;
        }

        return (s2 << 32) | s1;
    }
};

// Перечисление всех типов коллайдеров.
// - STATIC соответствует коллайдерам, с которыми могут сталкиваться другие
//   объекты, но при этом сам STATIC коллайдер от таких столкновений не
//   передвигается. Пример - стены или платформы.
// - DYNAMIC соответствует коллайдерам, которые могут передвигаться от
//   столкновений с другими телами. Также на них действует физика. Пример -
//   игрок, враги.
// - EVENT соответствует коллайдерам, на которые не влияют никакие другие
//   коллайдеры. Этот тип нужен для того, чтобы фиксировать столкновения
//   с нефизическими телами. Например - финиш уровня или пули.
enum class ColliderType
{
    STATIC,
    DYNAMIC,
    EVENT,
};

struct Collider
{
    bool enabled;
    float width, height;

    Collider() :
        enabled(false), types({}), width(0), height(0)
    {}

    // В параметр types передаётся множество тех типов, которым должен
    // соответствовать коллайдеры. Нескольким типам коллайдеров, например,
    // соответствует финиш уровня (STATIC и EVENT).
    Collider(
        Render &render,
        std::set<ColliderType> types = {ColliderType::STATIC}
    ) : enabled(true), types(types)
    {
        width = render.width * 1.0f / PIXEL_PER_UNIT;
        height = render.height * 1.0f / PIXEL_PER_UNIT;
    }

    bool of_type(ColliderType type)
    {
        return types.count(type) != 0;
    }
private:
    std::set<ColliderType> types;
};

// Структура, возвращаемая функцией CheckCollision. Поле exists равно false,
// если между соответствующими объектами есть нет пересечения. В таком случае
// overlap может быть произвольным.
//
// Поле exists равно true, если между объектами есть пересечение. В таком
// случае overlap показывает пересечение второго объекта относительно первого.
// При этом overlap отцентрирован относительно первого объекта.
//
// То есть если overlap.x < 0, то второй объект перекрывает первый по оси x
// слева относительно центра первого объекта на abs(overlap.x) метров. Иначе,
// аналогично, но перекрытие происходит справа.
//
// Аналогично это работает для оси y, но перекрытие происходит, соответственно,
// снизу и сверху.
struct Collision
{
    bool exists;
    Vector2 overlap;
};

struct Physics
{
    bool enabled;
    bool can_jump;
    float mass;
    Vector2 acceleration;
    Vector2 speed;

    Physics()
        : enabled(false), can_jump(false),
          mass(0), acceleration({0, 0}), speed({0, 0})
    {}
};

struct Bullet
{
    bool enabled;
    Vector2 speed;
    float lifetime;
    float max_lifetime;

    Bullet() :
        enabled(false),
        speed({0, 0}),
        lifetime(0),
        max_lifetime(0)
    {}

    Bullet(Vector2 speed, float max_lifetime) :
        enabled(true),
        speed(speed),
        lifetime(0),
        max_lifetime(max_lifetime)
    {}
};

enum class Direction
{
    LEFT,
    RIGHT
};

struct Player
{
    bool enabled;
    float speed;
    Direction direction;

    Player() :
        enabled(false),
        speed(0),
        direction(Direction::RIGHT)
    {}

    Player(float speed) :
        enabled(true),
        speed(speed),
        direction(Direction::RIGHT)
    {}
};

typedef void (*GUIDrawer)(Context &);

struct Finish
{
    bool enabled;
    Finish() : enabled(false) {}
};

struct Enemy
{
    bool enabled;
    float speed;
    Enemy() : enabled(false), speed(0.0f) {}
};

struct Object
{
    bool enabled;
    GameId id;
    Vector2 position;
    Render render;
    Collider collider;
    Physics physics;
    Bullet bullet;
    Player player;
    GUIDrawer gui_draw;
    Finish finish;
    Enemy enemy;

    Object() :
        enabled(true),
        position({0, 0}),
        render(Render()),
        collider(Collider()),
        physics(Physics()),
        bullet(Bullet()),
        player(Player()),
        gui_draw(nullptr),
        finish(Finish()),
        enemy(Enemy())
    {
        static GameId next_id = 0;
        this->id = next_id++;
    }

    friend bool operator==(const Object&, const Object&);
    friend bool operator!=(const Object&, const Object&);
};
