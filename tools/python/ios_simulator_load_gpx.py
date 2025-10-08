#!/usr/bin/env python3
"""
GPX to iOS Simulator simctl location command

Converts a GPX file to simctl location start command for realistic iOS location simulation.

Tested with CoMaps exported tracks

Usage:
    python gpx_to_simctl.py test_route.gpx
"""

import argparse
import xml.etree.ElementTree as ET
from pathlib import Path
import sys
import subprocess

def extract_track_points_from_gpx(gpx_file: Path):
    """Extract track points from GPX file."""
    tree = ET.parse(gpx_file)
    root = tree.getroot()
    
    points = []
    # Find all elements with lat/lon attributes
    for elem in root.findall('.//*[@lat][@lon]'):
        lat = float(elem.get('lat'))
        lon = float(elem.get('lon'))
        points.append((lat, lon))
    
    return points

def generate_simctl_command(points, speed_kmh=60, interval=0.1, distance=None, device="booted"):
    """Generate simctl location start command."""
    if len(points) < 2:
        raise ValueError("Need at least 2 waypoints for simctl location start")
    
    # Convert km/h to m/s
    speed_mps = speed_kmh / 3.6
    
    # Format waypoints as lat,lon pairs
    waypoint_strings = [f"{lat:.6f},{lon:.6f}" for lat, lon in points]
    
    # Build command
    cmd = ["xcrun", "simctl", "location", device, "start"]
    cmd.append(f"--speed={speed_mps:.2f}")
    
    if distance:
        cmd.append(f"--distance={distance}")
    else:
        cmd.append(f"--interval={interval}")
    
    cmd.extend(waypoint_strings)
    
    return cmd

def main():
    parser = argparse.ArgumentParser(
        description="Convert GPX file to simctl location start command",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python gpx_to_simctl.py test_route.gpx --speed 60 --interval 0.1
  python gpx_to_simctl.py test_route.gpx --speed 80 --distance 10 --clear-first
  python gpx_to_simctl.py test_route.gpx --speed 50 --dry-run
        """
    )
    
    parser.add_argument('gpx_file', help='Input GPX file')
    parser.add_argument('--speed', type=float, default=60, 
                       help='Speed in km/h (default: 60)')
    parser.add_argument('--interval', type=float, default=0.1,
                       help='Update interval in seconds (default: 0.1)')
    parser.add_argument('--distance', type=float,
                       help='Update distance in meters (overrides --interval)')
    parser.add_argument('--device', default='booted',
                       help='Target device (default: booted)')
    parser.add_argument('--dry-run', action='store_true',
                       help='Show command without executing (default: execute)')
    parser.add_argument('--clear-first', action='store_true',
                       help='Clear existing location before starting')
    
    args = parser.parse_args()
    
    # Validate input file
    gpx_file = Path(args.gpx_file)
    if not gpx_file.exists():
        print(f"Error: GPX file '{gpx_file}' not found", file=sys.stderr)
        return 1
    
    try:
        # Extract waypoints
        points = extract_track_points_from_gpx(gpx_file)
        print(f"Extracted {len(points)} waypoints from {gpx_file}")
        
        if len(points) < 2:
            print("Error: Need at least 2 waypoints for location simulation", file=sys.stderr)
            return 1
        
        # Generate command
        cmd = generate_simctl_command(
            points, 
            speed_kmh=args.speed, 
            interval=args.interval,
            distance=args.distance,
            device=args.device
        )
        
        # Show command
        print(f"\nGenerated simctl command:")
        print(" ".join(cmd))
        
        # Calculate simulation info
        speed_mps = args.speed / 3.6
        total_distance = 0
        for i in range(1, len(points)):
            lat1, lon1 = points[i-1]
            lat2, lon2 = points[i]
            # Simple distance approximation
            total_distance += ((lat2-lat1)**2 + (lon2-lon1)**2)**0.5 * 111000  # rough conversion to meters
        
        duration = total_distance / speed_mps
        print(f"\nSimulation info:")
        print(f"  Speed: {args.speed} km/h ({speed_mps:.1f} m/s)")
        print(f"  Waypoints: {len(points)}")
        print(f"  Estimated distance: {total_distance/1000:.2f} km")
        print(f"  Estimated duration: {duration:.0f} seconds ({duration/60:.1f} minutes)")
        if args.distance:
            print(f"  Update distance: {args.distance}m")
        else:
            print(f"  Update interval: {args.interval}s")
        
        # Execute by default unless dry-run
        if args.dry_run:
            print(f"\n[DRY RUN] Command that would be executed:")
            print(f"  {' '.join(cmd)}")
            if args.clear_first:
                clear_cmd = ["xcrun", "simctl", "location", args.device, "clear"]
                print(f"  (would clear location first: {' '.join(clear_cmd)})")
        else:
            print(f"\nExecuting command...")
            
            # Clear location first if requested
            if args.clear_first:
                clear_cmd = ["xcrun", "simctl", "location", args.device, "clear"]
                print("Clearing existing location...")
                subprocess.run(clear_cmd, check=True)
            
            # Execute the start command
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ Location simulation started successfully!")
                if result.stdout.strip():
                    print(result.stdout.strip())
            else:
                print(f"❌ Error executing command:")
                print(result.stderr.strip())
                return 1
        
        return 0
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())