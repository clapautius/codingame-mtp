#include <iostream>
#include <algorithm>
#include <time.h>
#include <stdlib.h>
#include "cgame-common.h" // :grep-out:
#include "params.h" // :grep-out:
#include "world.h" // :grep-out:
#include "bomb.h" // :grep-out:
#include "player.h" // :grep-out:
#include "tests.h" // :grep-out:

using namespace std;

static cgame::time_point_t g_start_time;

static bool g_battle_mode = false;

static vector<pair<int, int> > coords_around_2(int x, int y, bool include_center = false);

static bool location_safe_for_bomb_p(const Location &loc, const Bomb &b);

static Location g_initial_location;


struct Target
{
    Target(Location l = make_pair(0, 0), int t = -1, int c = 0)
      : location(l), type(static_cast<TargetType>(t)), score(c)
    {};

    bool is_valid() const
    {
        return type != -1;
    }

    void clear()
    {
        location = make_pair(-1, -1);
        type = TargetInvalid;
        score = 0;
    }

    string to_str() const
    {
        ostringstream ostr;
        ostr << "Target((" << location.first << "," << location.second << "),t=" << type
             << ")";
        return ostr.str();
    }

    Location location;

    TargetType type;

    int score;
};



Target compute_next_target_battle_mode_1();
Target compute_next_target_battle_mode_2();
Target compute_next_target_battle_mode_3();
static std::function<Target(void)> g_battle_mode_func = compute_next_target_battle_mode_2;
//static std::function<Target(void)> g_battle_mode_func = compute_next_target_battle_mode_1;


World g_world;

Player g_me;

vector<Player> g_other_players;

vector<Bomb> g_bombs;


int remaining_time()
{
    int remaining = 100 - cgame::diff_time_point_ms(g_start_time);
    cerr << "remaining time: " << remaining << endl;
    return remaining;
}


void read_data(int width, int height, int my_id)
{
    static bool first = true;
    g_bombs.clear();
    g_other_players.clear();
    int boxes = 0;
    for (int i = 0; i < height; i++) {
        string row;
        getline(cin, row);
/*
        cerr << row << endl; // :debug:
*/
        for (int j = 0; j < width; j++) {
            if (row[j] == '.') {
                g_world.matrix()[j][i] = EntityEmpty;
            } else if (row[j] == '0' || row[j] == '1' || row[j] == '2' || row[j] == '3') {
                g_world.matrix()[j][i] = EntityBox;
                ++boxes;
            } else if (row[j] == 'X') {
                g_world.matrix()[j][i] = EntityWall;
            } else {
                // this should not happen but hope for the best
                g_world.matrix()[j][i] = EntityEmpty;
            }
        }
    }
    g_world.set_no_boxes(boxes);
    int entities;
    cin >> entities; cin.ignore();
    for (int i = 0; i < entities; i++) {
        int entityType;
        int owner;
        int x;
        int y;
        int param1;
        int param2;
        cin >> entityType >> owner >> x >> y >> param1 >> param2; cin.ignore();
        //cerr << "got new entity: " << entityType << endl; // :debug:
        if (entityType == 0) {
            if (owner == my_id) {
                g_me.set_params(my_id, x, y, param1, param2, true);
                if (first) {
                    g_initial_location = make_pair(x, y);
                }
            } else {
                bool found = false;
                for (auto &player : g_other_players) {
                    if (player.id() == owner) {
                        player.set_params(owner, x, y, param1, param2);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    Player player(owner, x, y, param1);
                    g_other_players.push_back(player);
                }
                g_world.matrix()[x][y] = EntityEnemy;
            }
        } else if (entityType == 1) {
            g_world.matrix()[x][y] = EntityBombEnemy;
            Bomb b(owner, x, y, param1, param2);
            g_bombs.push_back(b);
            cerr << "bomb at " << x << ", " << y << " with timer "
                 << param1 << " and range " << param2 << endl;
        } else if (entityType == 2) {
            if (param1 == 2) {
                g_world.matrix()[x][y] = EntityGoodieExtraBomb;
            } else {
                g_world.matrix()[x][y] = EntityGoodieExtraRange;
            }
            cerr << "goodie at " << x << ", " << y << " with param1 "
                 << param1 << " and param2 " << param2 <<endl;
        }
    }
    if (first) {
        first = false;
    }
}


vector<pair<int, int> > coords_around(int x, int y, bool include_center)
{
    vector<pair<int, int> > result;
    if (include_center) {
        result.push_back(make_pair(x, y));
    }
    if (x > 0) {
        result.push_back(make_pair(x - 1, y));
    }
    if (x <= static_cast<int>(g_world.matrix().size()) - 2) {
        result.push_back(make_pair(x + 1, y));
    }
    if (y > 0) {
        result.push_back(make_pair(x, y - 1));
    }
    if (y <= static_cast<int>(g_world.matrix()[0].size()) - 2) {
        result.push_back(make_pair(x, y + 1));
    }
/*
    cerr << "valid around (" << x << "," << y << "):";
    for (auto &c : result) {
        cerr << "(" << c.first << "," << c.second << ") ";
    }
    cerr << endl;
*/
    return result;
}


/**
 * :fixme: there will be duplicates and it definitely can be improved.
 */
vector<pair<int, int> > coords_around_2(int x, int y, bool include_center)
{
    vector<Location> result;
    result = coords_around(x, y, include_center);
    vector<Location> extra_cells;
    for (auto &loc : result) {
        vector<Location> v = coords_around(loc.first, loc.second, false);
        extra_cells.insert(extra_cells.end(), v.begin(), v.end());
    }
    result.insert(result.end(), extra_cells.begin(), extra_cells.end());
    return result;
}


bool bomb_around_p(int x, int y)
{
    auto coords = coords_around(x, y);
    for (auto &c : coords) {
        if (g_world.entity(c.first, c.second) == EntityBombEnemy) {
            return true;
        }
    }
    return false;
}


pair<int, int> find_box(vector<vector<EntityType>> &m, int x, int y)
{
    cerr << "in find_box(" << x << "," << y << ")" << endl;
    if (m[x][y] == EntityBox) {
        return make_pair(x, y);
    } else {
        m[x][y] = EntityMark;
        vector<pair<int, int> > around = coords_around(x, y);
        for (auto &coord : around) {
            if (m[coord.first][coord.second] != EntityMark) {
                pair<int, int> result = find_box(m, coord.first, coord.second);
                if (result.first != -1) {
                    return result;
                }
            }
        }
        return make_pair(-1, -1);
    }
}


bool near_enemy_p(int x , int y)
{
    vector<pair<int, int> > around = coords_around(x, y);
    for (auto &coord : around) {
        if (g_world.matrix()[coord.first][coord.second] == EntityEnemy) {
            return true;
        }
    }
    return false;

}


bool near_box_p(int x, int y)
{
    vector<pair<int, int> > around = coords_around(x, y);
    for (auto &coord : around) {
        if (g_world.matrix()[coord.first][coord.second] == EntityBox) {
            return true;
        }
    }
    return false;
}


bool near_coord_p(int x, int y, int target_x, int target_y)
{
    if (x == target_x && y == target_y) {
        return true;
    }
    vector<pair<int, int> > around = coords_around(x, y);
    for (auto &coord : around) {
        if (coord.first == target_x && coord.second == target_y) {
            return true;
        }
    }
    return false;
}


/**
 * Compute Manhattan distance between two locations.
 */
int distance_between(const Location &loc1, const Location &loc2)
{
    return abs(loc1.first - loc2.first) + abs(loc1.second - loc2.second);
}


/**
 * :fixme: move to World class
 */
bool location_valid_p(int x, int y)
{
    return (x >= 0 && x < g_world.width() && y >= 0 && y < g_world.height());
}

/**
 * @return action_after : 0 - nothing, 1 - continue, 2 - break (1 - continue - not used
 * anymore)
 *
 * @param[out] boxes : updates the number of boxes
 */
int add_items_at(int x, int y, int &boxes, int &enemies, bool ignore_goodies = false)
{
    if (location_valid_p(x, y)) {
        EntityType e = g_world.entity(x, y);
        if (e == EntityBombEnemy) {
            // there is a bomb there, stop
            return 2;
        }
        if (e == EntityBox) {
            // only take into account boxes that are not in the range of other bombs
            if (g_world.get_explosion_timeout(x, y) == 9) {
                ++boxes;
            }
            return 2; // to break
        }
        if (e == EntityEnemy) {
            ++enemies;
            return 2;
        }
        if (ignore_goodies) {
            if (e == EntityWall) {
                // will block the explosion
                return 2;
            }
        } else {
            if (e == EntityWall || e == EntityGoodieExtraBomb || e == EntityGoodieExtraRange) {
                // will block the explosion
                return 2;
            }
        }
    }
    return 0;
}


void get_no_of_items_in_range(int x, int y, int &boxes, int &enemies)
{
    int rc = 0;
    bool ignore_goodies = false;
    // go left
    for (int i = x; i >= x - g_me.bomb_range() + 1; i--) {
        if (i == x) {
            ignore_goodies = true;
        } else {
            ignore_goodies = false;
        }
        rc = add_items_at(i, y, boxes, enemies, ignore_goodies);
        if (rc == 1) {
            continue;
        }
        if (rc == 2) {
            break;
        }
    }
    // go right
    for (int i = x + 1; i < x + g_me.bomb_range(); i++) {
        rc = add_items_at(i, y, boxes, enemies);
        if (rc == 1) {
            continue;
        }
        if (rc == 2) {
            break;
        }
    }
    // go up
    for (int i = y; i >= y - g_me.bomb_range() + 1; i--) {
        if (i == y) {
            ignore_goodies = true;
        } else {
            ignore_goodies = false;
        }
        rc = add_items_at(x, i, boxes, enemies, ignore_goodies);
        if (rc == 1) {
            continue;
        }
        if (rc == 2) {
            break;
        }
    }
    // go down
    for (int i = y + 1; i < y + g_me.bomb_range(); i++) {
        rc = add_items_at(x, i, boxes, enemies);
        if (rc == 1) {
            continue;
        }
        if (rc == 2) {
            break;
        }
    }
}

#if 0
bool bomb_near_p(int x, int y)
{
    // bomb near location (range = 1)
    bool bomb_near = false;
    if (bomb_around_p(x, y)) {
        bomb_near = true;
    }
    /*
    // check around 2 steps
    if (!bomb_near) {
        vector<Location> locations = coords_around(x, y);
        for (auto &loc : locations) {
            if (bomb_around_p(loc.first, loc.second)) {
                bomb_near = true;
                break;
            }
        }
    }
    */
    if (!bomb_near) {
        for (auto &b : g_bombs) {
            if (distance_between(make_pair(b.get_x(), b.get_y()), make_pair(x, y)) <= 3) {
                bomb_near = true;
                break;
            }
        }
    }
    return bomb_near;
}
#endif

/**
 * @global g_world
 */
void compute_score(int cur_x, int cur_y, int x, int y, int &score, int &target_type)
{
    if (!g_world.is_empty(x, y) || g_world.is_closed_area(x, y)) {
        // we cannot go there, ignore
        target_type = -1;
        cerr << "not empty (or closed)" << endl; // :debug:
        return;
    }
    int distance = g_world.distance_to(x, y);
    int boxes = 0;
    int enemies = 0;
    get_no_of_items_in_range(x, y, boxes, enemies);

    // Minimum factor for a box should be 25, otherwise sometimes it doesn't worth going
    // for boxes because they are too far - we want to avoid that
    // On the other hand, if we don't have bombs available, there's no point considering
    // boxes too valuable.
    score = boxes * (g_me.bombs_available() ? 25 : 5) - distance;

#if 0
    if (enemies > 0) {
        score += k_enemy_near_score;
        if (near_enemy_p(cur_x, cur_y)) {
            score += 101;
        }
    }
#endif

    if (bomb_around_p(x, y)) {
        score -= 150;
    }

#if 0
    if (bomb_near_p(x, y)) {
        score -= 150;
    }

    // check if safe for bombs
    for (auto &b : g_bombs) {
        if (!location_safe_for_bomb_p(make_pair(x, y), b)) {
            score -= 105;
        }
    }
#endif

    /*
    bool goodie = false;
    bool goodie_important = false;
    */
    if (g_world.entity(x, y) == EntityGoodieExtraBomb) {
        //goodie = true;
        if (g_me.bombs_max() >= k_extra_bombs_threshold) {
            score += k_goodie_not_important; // not that important
        } else {
            score += k_goodie_bomb_important; // important
            //goodie_important = true;
        }
        if (boxes == 0) {
            target_type = 1;
        }
    }
    if (g_world.entity(x, y) == EntityGoodieExtraRange) {
        //goodie = true;
        if (g_me.bomb_range() > k_extra_range_threshold) {
            score += k_goodie_not_important; // not that important
        } if (g_me.bomb_range() >= 8) {
            score += 1; // not important at all
        } else {
            score += k_goodie_range_important; // important
        }
        if (boxes == 0) {
            target_type = 1;
        }
    }

    if (boxes > 0 && g_me.bombs_available() > 0) {
        // distance is a factor only if we have bombs available
        if (distance <= k_close_distance_for_bonus) {
            score += k_bonus_for_close_cell;
        }
    }
    if (distance >= k_far_away) {
        score -= k_cost_for_far_away;
    }
    if (boxes > 0 || enemies > 0) {
        target_type = 0;
    } else {
        target_type = 1;
    }

    // :debug:
    cerr << "(" << x << "," << y << ")<" << boxes << "," << distance << "," << score << "> ";
}


// :fixme: - this is really incomplete
bool location_safe_for_bomb_p(const Location &loc, const Bomb &b)
{
    bool safe = true;
    int x = loc.first;
    int y = loc.second;

    bool obstacle = true;
    if (b.get_x() == x && abs(b.get_y() - y) <= b.get_range()) {
        obstacle = false;
        for (int i = min(b.get_y(), y); i <= max(b.get_y(), y); i++) {
            EntityType e = g_world.entity(x, i);
            if (e == EntityBox || e == EntityWall) {
                obstacle = true;
                break;
            }
        }
    }
    if (!obstacle) {
        safe = false;
    }
    if (safe) {
        obstacle = true;
        if (b.get_y() == y && abs(b.get_x() - x) <= b.get_range()) {
            obstacle = false;
            for (int i = min(b.get_x(), x); i <= max(b.get_x(), x); i++) {
                EntityType e = g_world.entity(i, y);
                if (e == EntityBox || e == EntityWall) {
                    obstacle = true;
                    break;
                }
            }
        }
        if (!obstacle) {
            safe = false;
        }
    }
    //cerr << "location " << x << ", " << y << " is "
    //     << (safe ? "safe" : "not safe") << " for bomb " << b.get_x() << ", "
    //     << b.get_y() << endl;
    return safe;
}


#if 0 // not used anymore
bool location_safe_p(const Location &loc)
{
    bool safe = true;
    int x = loc.first;
    int y = loc.second;
    for (auto &b : g_bombs) {
        if (!location_safe_for_bomb_p(loc, b)) {
            safe = false;
            break;
        }
    }
    cerr << "location " << x << ", " << y << " is "
         << (safe ? "safe" : "not safe") << endl;
    return safe;
}
#endif

bool check_iminent_explosion(Location &ret, const Target &target)
{
    ret = make_pair(-1, -1);
    bool fire_in_the_hole = false;
    Bomb detonating_bomb;
    int xx = g_me.get_x();
    int yy = g_me.get_y();
    for (auto &bomb : g_bombs) {
        if (bomb.timeout() == 2 || bomb.timeout() == 3) {
            bool bomb_is_mine = (bomb.get_owner() == g_me.id());
            if (!bomb_is_mine) {
                g_world.compute_explosion_access_zone(xx, yy);
            }
            if (bomb_is_mine ||
                g_world.is_explosion_accesible(bomb.get_x(), bomb.get_y())) {
                fire_in_the_hole = true;
                g_world.compute_explosions(g_bombs);
                break;
            } else {
                cerr << "bomb " << bomb.description()
                     << " is not accesible" << endl;
            }
        }
    }

    if (fire_in_the_hole) {
        // case 1 - all locations around are safe
        vector<Location> close_locations = coords_around(xx, yy, true);
        bool all_ok = true;
        for (auto &l : close_locations) {
            if (g_world.is_empty(l.first, l.second) &&
                g_world.get_explosion_timeout(l.first, l.second) <= 3) {
                all_ok = false;
                break;
            }
        }
        if (all_ok) {
            cerr << "everything ok for the next explosion" << endl;
            fire_in_the_hole = false;
        }

        // not all ok - try to find a safe spot (the closest one or the dest. of the next
        // target)
        int min_dist = 100;
        if (fire_in_the_hole) {
            vector<Location> locations = coords_around_2(xx, yy, true);
            for (auto &l : locations) {
                // if we've just placed a bomb, current location won't be empty
                // so we need the special condition
                if ((g_world.is_empty(l.first, l.second) ||
                     (l.first == g_me.get_x() && l.second == g_me.get_y()))
                    && g_world.is_accesible(l.first, l.second)
                    && g_world.get_explosion_timeout(l.first, l.second) > 3) {
                    int d = g_world.distance_to(l.first, l.second);
                    if (d < min_dist) {
                        min_dist = d;
                        ret = l;
                    }
                    if (target.is_valid() &&
                        l.first == target.location.first &&
                        l.second == target.location.second) {
                        cerr << "safe location is target dest." << endl;
                        min_dist = -1;
                        ret = l;
                        break;
                    }
                }
            }
        }

        if (fire_in_the_hole) {
            cerr << "safe location: " << ret.first << ", " << ret.second
                 << ", current loc: " << g_me.get_x() << ", " << g_me.get_y() << endl;
        }
    }
    return fire_in_the_hole;
}


bool have_exit_point_from_target(const Target &t)
{
    bool loc_safe = false;
    if (t.type == 0) { // bomb type
        for (int i = 0; i < g_world.width() && !loc_safe; i++) {
            for (int j = 0; j < g_world.height(); j++) {
                if (g_world.is_accesible(i, j)) {
                    Bomb b(0, t.location.first, t.location.second, 2,
                           g_me.bomb_range());
                    if (location_safe_for_bomb_p(make_pair(i, j), b)) {
                        loc_safe = true; // we have at least a safe location
                        break;
                    }
                }
            }
        }
    } else {
        loc_safe = true;
    }
    return loc_safe;
}


Target compute_next_target_battle_mode_1()
{
    Target target;
    target.location = make_pair(rand() % g_world.width(), rand() % g_world.height());
    target.type = TargetWalkAndBombs;
    target.score = 0;
    return target;
}


Target compute_next_target_battle_mode_2()
{
    Target target;
    // for the current game there's only one enemy
    target.location = make_pair(g_other_players[0].get_x(), g_other_players[0].get_y());
    target.type = TargetWalkAndBombs;
    target.score = k_almost_max_score;
    return target;
}


Target compute_next_target_battle_mode_3()
{
    Target target;
    // for the current game there's only one enemy
    target.location = make_pair(g_other_players[0].get_x(), g_other_players[0].get_y());
    target.type = TargetWalkAndBombs;
    target.score = k_almost_max_score;
    return target;
}


Target compute_next_target(bool ignore_current_position)
{
    vector<Target> targets;
    Target result;
    if (g_world.boxes() == 0) {
        g_battle_mode = true;
        // battle mode - add a single target (type 2)
        targets.push_back(g_battle_mode_func());
    } else {
        for (int i = 0; i < g_world.height(); i++) {
            cerr << "score(...," << i << "): "; // :debug:
            for (int j = 0; j < g_world.width(); j++) {
                if (!(ignore_current_position && j == g_me.get_x() && i == g_me.get_y())) {
                    if (g_world.is_accesible(j, i)) {
                        int target_type = -1;
                        int score;
                        compute_score(g_me.get_x(), g_me.get_y(), j, i, score, target_type);
                        if (target_type != -1) {
                            targets.push_back(Target(make_pair(j, i), target_type, score));
                        }
                    }
                }
            }
            cerr << endl; // :debug:
        }
        // sort by score
        sort(targets.begin(), targets.end(),
             [] (const Target &a, const Target &b) -> bool
             { return a.score < b.score; });
    }

    // take all targets and check if there are safe points from them
    while (targets.size() > 0) {
        Target &t = targets[targets.size() - 1];
        if (have_exit_point_from_target(t)) {
            break;
        }
        targets.pop_back();
    }

    if (targets.size() > 0) {
        result = targets[targets.size() - 1];
        cerr << "best target: " << result.location.first << ", "
             << result.location.second << ", type=" << result.type << ", score="
             << result.score << endl;
    }
    return result;
}


int my_bombs()
{
    int counter = 0;
    for (auto &b : g_bombs) {
        if (b.get_owner() == g_me.id()) {
            counter++;
        }
    }
    return counter;
}


/**
 * Places a bomb at the current location if everything is fine.
 *
 * @return true if the bomb has been placed.
 * @global g_me, g_world, g_bombs
 */
bool place_bomb(Target &t)
{
    // check potential explosion
    bool ok_to_bomb = true;
    int cur_x = g_me.get_x();
    int cur_y = g_me.get_y();
    for (auto &bomb : g_bombs) {
        if (bomb.timeout() <= 4) {
            g_world.compute_explosions(g_bombs);
            if (g_world.get_explosion_timeout(cur_x, cur_y) <= 4) {
                ok_to_bomb = false;
                break;
            }
        }
    }
    if (ok_to_bomb) {
        cerr << "i'm trying to place a bomb at " << cur_x << ", " << cur_y << endl;
        if (remaining_time() > 20) {
            g_world.matrix()[cur_x][cur_y] = EntityBombEnemy;
            Bomb b(g_me.id(), cur_x, cur_x);
            g_bombs.push_back(b);
            g_world.compute_closed_zone(g_initial_location.first,
                                        g_initial_location.second);
            Target t_new = compute_next_target(false);
            if (t_new.is_valid()) {
                cout << "BOMB " << t_new.location.first << " "
                     << t_new.location.second << endl;
            } else {
                cout << "BOMB " << cur_x << " " << cur_y << endl;
            }
        } else {
            t.type = TargetInvalid;
            cout << "BOMB " << cur_x << " " << cur_y << endl;
        }
    } else {
        cerr << "not ok to bomb" << endl;
    }
    return ok_to_bomb;
}

/**
 * Moves towards the specified position.
 *
 * @return true if the move command has been isssued.
 */
bool move_me(int x, int y, const std::string &msg = "")
{
    cout << "MOVE " << x << " " << y << " " << msg << endl;
    return true;
}


/**
 * Checks if target is valid.
 *
 * @param[in] cur_loc
 * @param[in,out] target
 * @param[in] emergency_target
 * @param[out] command_executed : true if a command has been executed.
 */
void execute_target(const Location &cur_loc, Target &target, const bool emergency_target,
                    Location &previous_loc, bool &command_executed, const string &text)
{
    cerr << "Executing target " << target.to_str() << "; text=" << text << endl;
    if (!target.is_valid()) {
        return;
    }
    // we have a target
    if (target.location == cur_loc) {
        cerr << "we are here" << endl;
#ifdef HYPER_DEBUG
        cerr << "target.type=" << target.type << ", g_me.bombs_available()="
             << g_me.bombs_available() << ", my_bombs()=" << my_bombs()
             << ", vital_space=" << g_world.vital_space() << endl;
#endif
        if ((target.type == 0 || target.type == 2) &&
            (g_me.bombs_available() &&
             (my_bombs() == 0  || g_world.vital_space() >= k_min_vital_space))) {
            Target new_target;
            command_executed = place_bomb(new_target);
        }
        if (emergency_target) {
            // stay
            command_executed = move_me(cur_loc.first, cur_loc.second, text);
        }
        target.type = TargetInvalid;
    } else {
        /*
        if (cur_loc == previous_loc && !emergency_target) {
            cerr << "Something is wrong, we cannot move. Abort" << endl;
            target.type = -1;
            previous_loc = make_pair(-1, -1);
        } else {
        */
        if (target.type == 2) {
            if (rand() % 3 == 0)
                /*&& !bomb_near_p(cur_loc.first, cur_loc.second)*/ {
                Target new_target; // :fixme: atm ignore
                command_executed = place_bomb(new_target);
            } else {
                command_executed = move_me(target.location.first,
                                           target.location.second, text);
            }
        } else {
            command_executed = move_me(target.location.first,
                                       target.location.second, text);
        }
            /*}*/
    }
}


/**
 * Changes the target if there's something interesting near me.
 *
 * @globals g_me, g_world.
 */
void search_near_me(Target &target)
{
    // first search near me
    int cur_x = g_me.get_x();
    int cur_y = g_me.get_y();
    int score = 0;
    int target_type = 0;
    bool ok_near_me = true;
    compute_score(cur_x, cur_y, cur_x, cur_y, score, target_type);
    if (target.type == TargetWalk) {
        ok_near_me = true;
    } else {
        if (score < k_min_score_for_bomb) {
            ok_near_me = false;
        }
        if (! (g_me.bombs_available() > 1 ||
               (target.is_valid() && g_me.bombs_available() > 0 && g_world.distance_to(target.location) >= 5))) {
            ok_near_me = false;
        }
    }
    if (ok_near_me) {
        Target test_target(make_pair(cur_x, cur_y), TargetBomb);
        if (have_exit_point_from_target(test_target)) {
            cerr << "box(es) in range - good enough" << endl;
            target = test_target;
        }
    }
}


void game_loop(int width, int height, int my_id)
{
    Target target;
    static bool first = true;
    static Location previous_loc = make_pair(-1, -1);
    while (1) {
        g_start_time = cgame::new_time_point();
        read_data(width, height, my_id);
        g_world.clear();
        g_world.compute_access_zone(g_me.get_x(), g_me.get_y());
        g_world.compute_closed_zone(g_initial_location.first, g_initial_location.second);
        g_world.compute_explosions(g_bombs);

        Location cur_loc = make_pair(g_me.get_x(), g_me.get_y());

        // first try to protect from explosions
        Location emergency_location;
        bool emergency_target = false;
        if (check_iminent_explosion(emergency_location, target)) {
            // preparing for explosion
            if (emergency_location.first != -1) {
                target.location = emergency_location;
                target.type = TargetWalk;
                emergency_target = true;
            }
        }

        // check own explosion and look again for target
        static bool search_again = false;
        if (search_again && !emergency_target) {
            target.clear();
            search_again = false;
            search_near_me(target);
        }

        for (auto bomb : g_bombs) {
            if (bomb.timeout() == 1) {
                // next turn abort the current target
                search_again = true;
                break;
            }
        }

        bool command_executed = false;

        // if we have the same position as the enemy, place a bomb
        if (!emergency_target &&
            (g_me.x() == g_other_players[0].x() && g_me.y() == g_other_players[0].y())) {
            target.clear();
            target.location = make_pair(g_me.x(), g_me.y());
            target.type = TargetBomb;
            target.score = 0;
            cerr << "placing a bomb near enemy" << endl;
        }

        if (target.is_valid()) {
            execute_target(cur_loc, target, emergency_target, previous_loc,
                           command_executed, "A");
        }
        if (!target.is_valid() && !command_executed) {
            target = compute_next_target(false);
            if (target.is_valid()) {
                execute_target(cur_loc, target, emergency_target, previous_loc,
                               command_executed, "B");
            } else {
                // just stay here
                command_executed = move_me(cur_loc.first, cur_loc.second, "d");
            }
        }
        // fail-safe
        if (!command_executed) {
            // just stay here
            cerr << "No command executed, not good" << endl;
            command_executed = move_me(cur_loc.first, cur_loc.second, "em");
        }
        if (first) {
            first = false;
        }
        previous_loc = cur_loc;
        cerr << "total time: " << cgame::diff_time_point_ms(g_start_time);
    } // end while(1)
}


// unit tests and other debug tests declarations
bool unit_tests();
bool test1();


/**
 * Auto-generated code below aims at helping you parse
 * the standard input according to the problem statement.
 **/
int main(int argc, char *argv[])
{
    if (argc > 1 && string(argv[1]) == "--tests") {
        exit((unit_tests() && cgame::unit_tests_common()) ? 0 : 1);
    }

    int width;
    int height;
    int myId;
    cin >> width >> height >> myId; cin.ignore();
    cerr << "width=" << width << ", height: " << height << ", myId=" << myId << endl;
    g_world.set_world_size(width, height);
    srand(time(NULL));

    game_loop(width, height, myId);

    //  Unit tests.
    /*
    test1();
    */
}
