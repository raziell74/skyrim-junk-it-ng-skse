#!/usr/bin/env python3
"""
Fix Skyrim MCM Translation File Encoding

This script ensures all translation files in papyrus/Interface/translations/
have the required UTF-16 LE BOM (Byte Order Mark) that Skyrim needs to
properly parse MCM translation files.

Without the BOM, Skyrim displays translation keys (e.g., $JunkItMCM)
instead of the actual translated text.
"""

import os
from pathlib import Path

BOM_UTF16_LE = b'\xff\xfe'

def fix_translation_files(translations_dir='papyrus/Interface/translations'):
    """
    Scan translation files and add UTF-16 LE BOM if missing.
    
    Args:
        translations_dir: Path to translations directory
        
    Returns:
        Tuple of (files_fixed, files_already_ok, files_total)
    """
    script_dir = Path(__file__).parent
    trans_path = script_dir / translations_dir
    
    if not trans_path.exists():
        print(f"Error: Translation directory not found: {trans_path}")
        return 0, 0, 0
    
    txt_files = list(trans_path.glob('*.txt'))
    
    if not txt_files:
        print(f"No .txt files found in {trans_path}")
        return 0, 0, 0
    
    files_fixed = []
    files_already_ok = []
    
    for file_path in txt_files:
        try:
            with open(file_path, 'rb') as f:
                content = f.read()
            
            if not content:
                print(f"Warning: {file_path.name} is empty, skipping")
                continue
            
            if not content.startswith(BOM_UTF16_LE):
                with open(file_path, 'wb') as f:
                    f.write(BOM_UTF16_LE + content)
                files_fixed.append(file_path.name)
            else:
                files_already_ok.append(file_path.name)
                
        except Exception as e:
            print(f"Error processing {file_path.name}: {e}")
    
    return files_fixed, files_already_ok, len(txt_files)

def main():
    print("=" * 60)
    print("Skyrim Translation File Encoding Fix")
    print("=" * 60)
    print()
    
    files_fixed, files_already_ok, total = fix_translation_files()
    
    if files_fixed:
        print(f"[FIXED] {len(files_fixed)} file(s) by adding UTF-16 LE BOM:")
        for filename in sorted(files_fixed):
            print(f"  - {filename}")
        print()
    
    if files_already_ok:
        print(f"[OK] {len(files_already_ok)} file(s) already had correct encoding:")
        for filename in sorted(files_already_ok):
            print(f"  - {filename}")
        print()
    
    if total > 0:
        print(f"Summary: {len(files_fixed)} fixed, {len(files_already_ok)} already OK, {total} total")
        print()
        if files_fixed:
            print("The translation files should now display correctly in Skyrim!")
    else:
        print("No translation files found to process.")
    
    print("=" * 60)

if __name__ == '__main__':
    main()
