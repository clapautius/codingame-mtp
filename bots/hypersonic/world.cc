#include <deque>
#include "world.h" // :grep-out:
#include "bomb.h" // :grep-out:

using std::vector;
using std::pair;
using std::deque;

void World::compute_access_zone(int start_x, int start_y)
{
    m_vital_space = 0;
    m_access_matrix.clear();
    m_access_matrix = m_matrix;
    compute_access_zone_rec(start_x, start_y);
}


void World::compute_explosion_access_zone(int start_x, int start_y)
{
    m_explosion_access_matrix.clear();
    m_explosion_access_matrix = m_matrix;
    compute_explosion_access_zone_rec(start_x, start_y);
}


void World::compute_access_zone_rec(int x, int y)
{
    m_access_matrix[x][y] = EntityMark;
    m_vital_space++;
    vector<pair<int, int> > around = coords_around(x, y);
    for (auto &coord : around) {
        if (m_access_matrix[coord.first][coord.second] != EntityMark) {
            if (is_empty(coord.first, coord.second)) {
                compute_access_zone_rec(coord.first, coord.second);
            }
        }
    }
}


void World::compute_explosion_access_zone_rec(int x, int y)
{
    m_explosion_access_matrix[x][y] = EntityMark;
    vector<pair<int, int> > around = coords_around(x, y);
    for (auto &coord : around) {
        if (m_explosion_access_matrix[coord.first][coord.second] != EntityMark) {
            if (is_empty_or_bomb(coord.first, coord.second)) {
                compute_explosion_access_zone_rec(coord.first, coord.second);
            }
        }
    }
}


int World::get_explosion_timeout(int x, int y) const
{
    return m_explosion_matrix[x][y];
}


Bomb& find_bomb_with_coords(vector<Bomb> &bombs, int x, int y)
{
    for (Bomb &bomb : bombs) {
        if (bomb.get_x() == x && bomb.get_y() == y) {
            return bomb;
        }
    }
    // we shouldn't be here
    throw std::string("Cannot find bomb with the specified coords");
}


/**
 * Computes a matrix containing explosion times for every cell.
 */
void World::compute_explosions(vector<Bomb> &world_bombs)
{
    // clear matrix (:fixme: optimize)
    for (int i = 0; i < width(); i++) {
        for (int j = 0; j < height(); j++) {
            m_explosion_matrix[i][j] = 9;
        }
    }
    deque<Bomb> bombs;
    // copy to deque
    for (auto &bomb : world_bombs) {
        bombs.push_back(bomb);
    }
    while (!bombs.empty()) {
        Bomb &bomb = bombs.front();
        bombs.pop_front();
        vector<Location> locations = bomb.get_explosion_area(*this);
        for (Location &loc : locations) {
            int xx = loc.first;
            int yy = loc.second;
            if (m_explosion_matrix[xx][yy] > bomb.effective_timeout()) {
                m_explosion_matrix[xx][yy] = bomb.effective_timeout();
            }
            if (bomb.get_x() != xx || bomb.get_y() != yy) {
                // don't check the same bomb
                if (entity(xx, yy) == EntityBombEnemy) {
                    Bomb &other_bomb = find_bomb_with_coords(world_bombs, xx, yy);
#ifdef HYPER_DEBUG
                    cerr << ":debug: found a new bomb on path: "
                         << other_bomb.description() << endl;
#endif
                    if (bomb.effective_timeout() < other_bomb.effective_timeout()) {
                            other_bomb.set_effective_timeout(bomb.effective_timeout());
                            bombs.push_back(other_bomb);
                    }
                }
            }
        }
    }
}


bool World::is_accesible(int x, int y) const
{
    return m_access_matrix[x][y] == EntityMark;
}


bool World::is_explosion_accesible(int x, int y) const
{
    return m_explosion_access_matrix[x][y] == EntityMark;
}
