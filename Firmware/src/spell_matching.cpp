/*
================================================================================
  Spell Matching - Gesture Pattern Recognition Implementation
================================================================================
  
  This module implements the core pattern matching algorithm that compares
  user-drawn wand gestures against known spell patterns.
  
  Algorithm Overview:
    1. Normalization: Scale gesture to 0-1000 coordinate space (scale/translation invariant)
    2. Resampling: Convert to fixed 40 points evenly distributed along path (length invariant)
    3. Position Similarity: Calculate average Euclidean distance between corresponding points
    4. Direction Similarity: Compare angles between consecutive point vectors
    5. Combined Score: 60% position + 40% direction weighted average
  
  Key Features:
    - Scale invariant: Large and small gestures match if shape is the same
    - Translation invariant: Location in tracking space doesn't matter
    - Length invariant: Fast and slow gestures match if shape is the same
    - Rotation partially addressed through direction similarity
  
  Similarity Scoring:
    - Position: Lower average distance = higher similarity
    - Direction: More parallel strokes = higher similarity
    - Final score: 0.0 (no match) to 1.0 (perfect match)
    - Threshold: 0.70 (70% similarity required for successful match)  
================================================================================
*/
#include "spell_matching.h"
#include <Arduino.h>
#include <cmath>

/**
 * Normalize trajectory to 0-1000 coordinate space
 * This function makes gesture recognition scale and translation invariant by:
 * - Finding the bounding box of all points in the trajectory
 * - Scaling all points proportionally to fit in a 1000x1000 space
 * - Translating so the minimum X,Y becomes (0,0)
 * Example: A small gesture in the corner and a large gesture in the center
 * will both normalize to the same shape in 0-1000 space if they have the
 * same proportions.
 * traj: The raw trajectory points from IR tracking
 * return Normalized trajectory with coordinates in 0-1000 range
 */
std::vector<Point> normalizeTrajectory(const std::vector<Point>& traj) {
  if (traj.size() < 2) return traj;
  
  // Find bounding box - the smallest rectangle that contains all points
  int minX = traj[0].x, maxX = traj[0].x;
  int minY = traj[0].y, maxY = traj[0].y;
  
  for (const auto& p : traj) {
    minX = min(minX, p.x);
    maxX = max(maxX, p.x);
    minY = min(minY, p.y);
    maxY = max(maxY, p.y);
  }
  
  // Calculate bounding box dimensions (avoid division by zero for straight lines)
  int width = max(1, maxX - minX);
  int height = max(1, maxY - minY);
  
  // Normalize to 0-1000 space for better precision than 0-1 floats
  // This gives us integer math while maintaining 1000 levels of precision
  std::vector<Point> normalized;
  for (const auto& p : traj) {
    Point np;
    // Scale: subtract min to translate to origin, multiply by 1000, divide by dimension
    np.x = ((p.x - minX) * 1000) / width;
    np.y = ((p.y - minY) * 1000) / height;
    // Normalize timestamps relative to first point (for potential speed analysis)
    np.timestamp = p.timestamp - traj[0].timestamp;
    normalized.push_back(np);
  }
  
  return normalized;
}

/**
 * Resample trajectory to a fixed number of evenly-spaced points
 * This function ensures all gestures have the same number of points regardless
 * of how fast they were drawn or how many samples were captured. Points are
 * distributed evenly along the path length, not by time.
 * Example: A slowly-drawn circle might have 500 captured points, while a fast
 * circle might have only 50 points. Both will resample to 20 evenly-spaced
 * points along the circumference.
 * Algorithm:
 * 1. Calculate total path length by summing distances between consecutive points
 * 2. Divide into equal segments (total length / desired points)
 * 3. Walk along the path, placing new points at each segment boundary
 * 4. Interpolate between original points when segment boundary falls between them
 * traj: The trajectory to resample (should be normalized first)
 * numPoints: The desired number of points (typically 20 for spell matching)
 * return Resampled trajectory with exactly numPoints evenly-spaced points
 */
std::vector<Point> resampleTrajectory(const std::vector<Point>& traj, int numPoints) {
  if (traj.size() < 2) return traj;
  
  // Calculate total path length by summing all segment distances
  // This gives us the "arc length" of the gesture
  float totalLength = 0;
  for (size_t i = 1; i < traj.size(); i++) {
    float dx = traj[i].x - traj[i-1].x;
    float dy = traj[i].y - traj[i-1].y;
    totalLength += sqrt(dx*dx + dy*dy);  // Euclidean distance
  }
  
  // Calculate how far apart each new point should be along the path
  // We want numPoints total, so (numPoints - 1) segments between them
  float segmentLength = totalLength / (numPoints - 1);
  
  std::vector<Point> resampled;
  resampled.push_back(traj[0]);  // Always keep the first point
  
  float distanceSoFar = 0;  // Distance accumulated along current segment
  
  // Walk through each segment of the original trajectory
  for (size_t i = 1; i < traj.size() && resampled.size() < numPoints; i++) {
    float dx = traj[i].x - traj[i-1].x;
    float dy = traj[i].y - traj[i-1].y;
    float segDist = sqrt(dx*dx + dy*dy);  // Length of current segment
    
    // Check if we should place one or more new points within this segment
    // (handles case where multiple resampled points fall on one original segment)
    while (distanceSoFar + segDist >= segmentLength && resampled.size() < numPoints) {
      // Calculate where along this segment the new point should be
      // ratio = 0 means at start of segment, ratio = 1 means at end
      float ratio = (segmentLength - distanceSoFar) / segDist;
      
      // Linearly interpolate between the two points
      Point p;
      p.x = traj[i-1].x + ratio * dx;
      p.y = traj[i-1].y + ratio * dy;
      p.timestamp = traj[i-1].timestamp + ratio * (traj[i].timestamp - traj[i-1].timestamp);
      resampled.push_back(p);
      
      // Reset distance counter and adjust remaining segment length
      distanceSoFar = 0;
      segDist = (1 - ratio) * segDist;  // Remaining distance in this segment
    }
    
    // Accumulate leftover distance for next iteration
    distanceSoFar += segDist;
  }
  
  // Ensure we have exactly numPoints (edge case: rounding errors might leave us one short)
  if (resampled.size() < numPoints) {
    resampled.push_back(traj.back());
  }
  
  return resampled;
}

/**
 * Calculate direction similarity between two trajectories
 * This measures how well the directional flow of two gestures matches.
 * Two gestures could have points in similar positions but with opposite
 * directions (e.g., clockwise vs counter-clockwise circle).
 * Algorithm:
 * 1. For each pair of consecutive points, calculate direction angle using atan2
 * 2. Compare angles between corresponding segments in both trajectories
 * 3. Average the angle differences and normalize to 0-1 score
 * The angle difference is wrapped to handle the -π to +π boundary
 * (e.g., -179° and +179° are only 2° apart, not 358° apart)
 * traj1: First trajectory (resampled to same length as traj2)
 * traj2: Second trajectory (resampled to same length as traj1)
 * return Similarity score from 0 (opposite directions) to 1 (identical directions)
 */
float calculateDirectionSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2) {
  if (traj1.size() != traj2.size() || traj1.size() < 2) return 0;
  
  float totalAngleDiff = 0;
  int numSegments = traj1.size() - 1;
  
  // Compare each segment's direction
  for (size_t i = 0; i < numSegments; i++) {
    // Calculate direction vectors for corresponding segments
    // These represent the direction of movement between consecutive points
    float dx1 = traj1[i+1].x - traj1[i].x;
    float dy1 = traj1[i+1].y - traj1[i].y;
    float dx2 = traj2[i+1].x - traj2[i].x;
    float dy2 = traj2[i+1].y - traj2[i].y;
    
    // Calculate angles in radians (-π to +π)
    // atan2(y, x) gives the angle from positive X-axis
    float angle1 = atan2(dy1, dx1);
    float angle2 = atan2(dy2, dx2);
    
    // Calculate angle difference, handling wrap-around at ±π
    // Example: angle1 = +170°, angle2 = -170°
    // Direct difference = 340°, but wrapped difference = 20°
    float angleDiff = abs(angle1 - angle2);
    if (angleDiff > M_PI) {
      angleDiff = 2 * M_PI - angleDiff;  // Wrap to shorter arc
    }
    
    totalAngleDiff += angleDiff;
  }
  
  // Average angle difference across all segments
  float avgAngleDiff = totalAngleDiff / numSegments;
  
  // Normalize to 0-1 score
  // avgAngleDiff ranges from 0 (perfect match) to π (opposite directions)
  // Subtract from 1 so higher score = better match
  float similarity = 1.0 - (avgAngleDiff / M_PI);
  
  return max(0.0f, similarity);
}

/**
 * Calculate overall similarity between two trajectories
 * This is the main similarity metric that combines two aspects:
 * 1. Position similarity - how close corresponding points are to each other
 * 2. Direction similarity - how well the directional flow matches
 * The combination (60% position, 40% direction) gives more weight to shape
 * while still ensuring directional correctness. This prevents false matches
 * like confusing a clockwise circle with a counter-clockwise one.
 * Both trajectories must be normalized and resampled to the same number of
 * points before calling this function.
 * traj1: First trajectory (normalized and resampled)
 * traj2: Second trajectory (normalized and resampled)
 * return Similarity score from 0 (completely different) to 1 (identical)
 */
float calculateSimilarity(const std::vector<Point>& traj1, const std::vector<Point>& traj2) {
  if (traj1.size() != traj2.size() || traj1.empty()) return 0;
  
  // Calculate position similarity by measuring point-to-point distances
  float totalDistance = 0;
  float maxPossibleDistance = 1414.0; // Diagonal of 1000x1000 normalized space (sqrt(1000² + 1000²))
  
  // Sum distances between corresponding points
  for (size_t i = 0; i < traj1.size(); i++) {
    float dx = traj1[i].x - traj2[i].x;
    float dy = traj1[i].y - traj2[i].y;
    totalDistance += sqrt(dx*dx + dy*dy);  // Euclidean distance
  }
  
  // Average the distances and normalize to 0-1 range
  float avgDistance = totalDistance / traj1.size();
  float positionSimilarity = 1.0 - (avgDistance / maxPossibleDistance);
  
  // Calculate direction similarity (how well the flow/direction matches)
  float directionSimilarity = calculateDirectionSimilarity(traj1, traj2);
  
  // Combined score: weighted average
  // 60% position (shape) + 40% direction (flow)
  // This weighting was chosen to prioritize overall shape while still
  // distinguishing between gestures drawn in opposite directions
  float combinedSimilarity = (positionSimilarity * 0.6) + (directionSimilarity * 0.4);
  
  return max(0.0f, combinedSimilarity);
}

/**
 * Attempt to match a drawn gesture against all known spell patterns
 * This is the main entry point for spell recognition. It takes a raw trajectory
 * from the IR camera and compares it against all predefined spell patterns to
 * find the best match.
 * Process:
 * 1. Validate trajectory has minimum points
 * 2. Normalize and resample user's gesture to standard form
 * 3. Compare against each spell pattern (also normalized and resampled)
 * 4. Find the spell with highest similarity score
 * 5. Check if best match exceeds threshold (defined in spell_matching.h)
 * 6. Output result to serial for debugging and UI display
 * currentTrajectory: The raw trajectory points captured from IR tracking
 */
void matchSpell(const std::vector<Point>& currentTrajectory) {
  // Reject trajectories that are too short to be meaningful gestures
  if (currentTrajectory.size() < MIN_TRAJECTORY_POINTS) {
    Serial.println("SPELL: Too short");
    return;
  }
  
  // Prepare the user's gesture for comparison:
  // - Normalize to standard 0-1000 coordinate space (scale/translation invariant)
  // - Resample to exactly 40 evenly-spaced points (consistent comparison)
  std::vector<Point> normalized = normalizeTrajectory(currentTrajectory);
  std::vector<Point> resampled = resampleTrajectory(normalized, 40);
  
  // Calculate gesture duration for debugging/display
  // (not used in matching - we removed speed constraints)
  uint32_t duration = currentTrajectory.back().timestamp - currentTrajectory.front().timestamp;
  
  // Compare against all known spell patterns
  // Find the spell with the highest similarity score
  // Note: Spell patterns are already normalized and resampled to 40 points during initialization
  float bestMatch = 0;
  const char* bestSpell = "Unknown";
  
  Serial.println("=== Spell Matching Results ===");
  
  for (const auto& spell : spellPatterns) {
    // Calculate position similarity separately for diagnostics
    float totalDistance = 0;
    float maxPossibleDistance = 1414.0;
    for (size_t i = 0; i < resampled.size(); i++) {
      float dx = resampled[i].x - spell.pattern[i].x;
      float dy = resampled[i].y - spell.pattern[i].y;
      totalDistance += sqrt(dx*dx + dy*dy);
    }
    float avgDistance = totalDistance / resampled.size();
    float positionSimilarity = 1.0 - (avgDistance / maxPossibleDistance);
    
    // Calculate direction similarity for diagnostics
    float directionSimilarity = calculateDirectionSimilarity(resampled, spell.pattern);
    
    // Combined similarity
    float similarity = (positionSimilarity * 0.6) + (directionSimilarity * 0.4);
    
    // Print detailed breakdown
    Serial.printf("  %s: %.2f%% (pos: %.2f%%, dir: %.2f%%)\n", 
                  spell.name, similarity * 100, positionSimilarity * 100, directionSimilarity * 100);
    
    // Track the best match found so far
    if (similarity > bestMatch) {
      bestMatch = similarity;
      bestSpell = spell.name;
    }
  }
  
  Serial.println("==============================");
  
  // Output result to serial for debugging
  // MATCH_THRESHOLD is typically 0.7 (70% similarity required)
  if (bestMatch >= MATCH_THRESHOLD) {
    Serial.printf("SPELL: %s (%.2f%% match, %d points, %dms)\n", 
                  bestSpell, bestMatch * 100, currentTrajectory.size(), duration);
  } else {
    Serial.printf("SPELL: No match (best: %s %.2f%%)\n", bestSpell, bestMatch * 100);
  }
}
