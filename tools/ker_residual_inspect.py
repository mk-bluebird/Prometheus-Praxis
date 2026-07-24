#!/usr/bin/env python3
"""
KER and Residual Inspection Tool

A minimal Python stdlib script to inspect vshardker and vshardresidual views.
Connects to db/cyboquatic_core.db and prints summary statistics per plane.
"""

import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(os.path.dirname(__file__)), "db", "cyboquatic_core.db")


def main():
    """Connect to the database and print KER and residual summaries."""
    if not os.path.exists(DB_PATH):
        print(f"Database not found at {DB_PATH}")
        return

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    print("=== KER Score Summary by Plane ===")
    try:
        cursor.execute("""
            SELECT plane, AVG(ker_score) AS avg_ker_score
            FROM vshardker
            GROUP BY plane
        """)
        rows = cursor.fetchall()
        if rows:
            for plane, avg_ker in rows:
                print(f"  {plane}: avg_ker_score = {avg_ker}")
        else:
            print("  (no data in vshardker)")
    except sqlite3.Error as e:
        print(f"  Error querying vshardker: {e}")

    print("\n=== Residual Delta Summary by Plane ===")
    try:
        cursor.execute("""
            SELECT plane, AVG(delta_vt) AS avg_delta_vt
            FROM vshardresidual
            GROUP BY plane
        """)
        rows = cursor.fetchall()
        if rows:
            for plane, avg_delta in rows:
                print(f"  {plane}: avg_delta_vt = {avg_delta}")
        else:
            print("  (no data in vshardresidual)")
    except sqlite3.Error as e:
        print(f"  Error querying vshardresidual: {e}")

    conn.close()
    print("\nInspection complete.")


if __name__ == "__main__":
    main()
