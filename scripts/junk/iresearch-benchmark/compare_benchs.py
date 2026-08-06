#!/usr/bin/python3

import re
import sys
import argparse
from pathlib import Path
from typing import List

class TestResult:
    def __init__(self):
        self.search_results: List[int] = []
    
    def parse_from_text(self, text: str) -> None:
        lines = text.strip().split('\n')
        
        results_section = False
        for i, line in enumerate(lines):
            if 'Test search results:' in line:
                results_section = True
                continue
            if results_section and line.strip().isdigit():
                self.search_results.append(int(line.strip()))
            if results_section and ('Test \'single_search\'' in line or 
                                   'Test \'freqs_search\'' in line or 
                                   'All tests' in line):
                results_section = False

def parse_file(filepath: str) -> TestResult:
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    result = TestResult()
    result.parse_from_text(content)
    return result


def compare_results(file1: str, file2: str):
    result1 = parse_file(file1)
    result2 = parse_file(file2)
    
    if result1.search_results and result2.search_results:
        max_len = min(len(result1.search_results), len(result2.search_results))
        for i in range(max_len):
            diff = result1.search_results[i] - result2.search_results[i]
            if diff:
                raise Exception(f"{file1} and {file2} has diff in result {i}")
            
        
        if len(result1.search_results) != len(result2.search_results):
            print(f"\nWarning: Different number of results ({len(result1.search_results)} vs {len(result2.search_results)})")


def main():
    parser = argparse.ArgumentParser(description='Compare test results from two files')
    parser.add_argument('dir1', help='First dir with test results')
    parser.add_argument('dir2', help='Second dir with test results')
    
    args = parser.parse_args()

    dir1 = Path(args.dir1)
    dir2 = Path(args.dir2)
    
    if not dir1.exists():
        print(f"Error: Dir '{dir1}' not found")
        sys.exit(1)
    if not dir2.exists():
        print(f"Error: Dir '{dir2}' not found")
        sys.exit(1)


    for file1 in dir1.iterdir():
        name = file1.name
        file2 = dir2 / name
        compare_results(file1, file2)

if __name__ == "__main__":
    main()
