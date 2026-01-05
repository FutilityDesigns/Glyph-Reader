#ifndef SPELL_MATCHING_H
#define SPELL_MATCHING_H

#include "spell_patterns.h"
#include <vector>

// Spell detection parameters
#define MIN_TRAJECTORY_POINTS 50
#define MATCH_THRESHOLD 0.70   // Similarity threshold (0-1, higher = stricter)

// Normalize trajectory to 0-1000 coordinate space
std::vector<Point> normalizeTrajectory(const std::vector<Point>& traj);

// Resample trajectory to fixed number of points
std::vector<Point> resampleTrajectory(const std::vector<Point>& traj, int numPoints);

// Calculate direction similarity between two trajectories (0-1, 1 = perfect match)
float calculateDirectionSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2);

// Calculate similarity between two trajectories (0-1, 1 = perfect match)
float calculateSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2);

// Attempt to match gesture against known spells
void matchSpell(const std::vector<Point>& currentTrajectory);

#endif
