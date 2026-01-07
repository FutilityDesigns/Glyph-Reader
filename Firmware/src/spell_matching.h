/*
================================================================================
  Spell Matching - Gesture Pattern Recognition Header
================================================================================
  
  This module implements the pattern matching algorithm that compares recorded
  wand gestures against known spell patterns.
  
  Algorithm Overview:
    1. Normalize trajectory to 0-1000 coordinate space (scale invariant)
    2. Resample trajectory to fixed 40 points (length invariant)
    3. Calculate position similarity (Euclidean distance)
    4. Calculate direction similarity (angle between consecutive points)
    5. Weighted combination: 60% position + 40% direction
  
  Tunable Parameters:
    - MIN_TRAJECTORY_POINTS: Minimum points required for valid gesture (50)
    - MATCH_THRESHOLD: Similarity threshold for successful match (0.70 = 70%)
================================================================================
*/

#ifndef SPELL_MATCHING_H
#define SPELL_MATCHING_H

#include "spell_patterns.h"
#include <vector>

//=====================================
// Spell Detection Parameters
//=====================================

/**
 * Minimum trajectory points required for valid gesture
 * Gestures with fewer points are rejected as "too short".
 * Prevents accidental triggers from brief IR detections.
 */
#define MIN_TRAJECTORY_POINTS 50

/**
 * Similarity threshold for spell matching (0.0 to 1.0)
 * Gesture must score at least this value to be considered a match.
 * Higher values = stricter matching, fewer false positives.
 * Lower values = more lenient matching, more false positives.
 * Current: 0.70 (70% similarity required)
 */
#define MATCH_THRESHOLD 0.70

//=====================================
// Trajectory Processing Functions
//=====================================

/**
 * Normalize trajectory to standard 0-1000 coordinate space
 * Scales trajectory to fit within a 1000x1000 bounding box while preserving
 * aspect ratio. This makes matching scale-invariant (large and small gestures
 * match equally if shape is the same).
 * traj: Input trajectory with raw camera coordinates
 * return Normalized trajectory in 0-1000 space
 */
std::vector<Point> normalizeTrajectory(const std::vector<Point>& traj);

/**
 * Resample trajectory to fixed number of points via interpolation
 * Uses linear interpolation to create a trajectory with exactly numPoints.
 * This makes matching length-invariant (fast and slow gestures match equally).
 * traj: Input trajectory (any number of points)
 * numPoints: Desired number of points in output (typically 40)
 * return Resampled trajectory with exactly numPoints
 */
std::vector<Point> resampleTrajectory(const std::vector<Point>& traj, int numPoints);

/**
 * Calculate direction similarity between two trajectories
 * Compares the angle between consecutive points in each trajectory.
 * Returns value from 0 to 1, where 1 = perfectly parallel strokes.
 * traj1: First trajectory (normalized and resampled)
 * traj2: Second trajectory (normalized and resampled)
 * return Direction similarity score (0.0 to 1.0)
 */
float calculateDirectionSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2);

/**
 * Calculate combined similarity between two trajectories
 * Combines position-based and direction-based similarity:
 *   - Position: Average Euclidean distance between corresponding points
 *   - Direction: Angle similarity between consecutive point vectors
 *   - Final score: 60% position + 40% direction
 * traj1: First trajectory (normalized and resampled)
 * traj2: Second trajectory (normalized and resampled)
 * return Combined similarity score (0.0 to 1.0, 1 = perfect match)
 */
float calculateSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2);

//=====================================
// Main Matching Function
//=====================================

/**
 * Match gesture against all known spell patterns
 * Process:
 *   1. Check if trajectory has enough points (MIN_TRAJECTORY_POINTS)
 *   2. Normalize and resample trajectory
 *   3. Compare against all spell patterns in spellPatterns vector
 *   4. Calculate similarity score for each pattern
 *   5. Find best match above MATCH_THRESHOLD
 *   6. Trigger spell actions (LED, display, MQTT) if matched
 * Outputs detailed diagnostics to serial console showing similarity scores
 * for all patterns (helpful for tuning and debugging).
 * currentTrajectory: Recorded gesture trajectory from camera
 */
void matchSpell(const std::vector<Point>& currentTrajectory);

#endif // SPELL_MATCHING_H
