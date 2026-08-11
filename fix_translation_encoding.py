#!/usr/bin/env python3
"""
Fix Skyrim MCM Translation File Encoding

This script ensures all translation files in papyrus/Interface/translations/
are stored as UTF-16 LE with BOM (Byte Order Mark), which Skyrim requires to
properly parse MCM translation files.

Without correct UTF-16 LE + BOM, Skyrim displays translation keys (e.g.,
$JunkItMCM) instead of the actual translated text.

Files saved as UTF-8 (or other encodings) are decoded and re-encoded to
UTF-16 LE; prepending BOM bytes alone would corrupt non–UTF-16 source files.
"""

from pathlib import Path

BOM_UTF16_LE = b'\xff\xfe'
BOM_UTF16_BE = b'\xfe\xff'
BOM_UTF8 = b'\xef\xbb\xbf'


def _decode_translation_bytes(raw: bytes) -> str:
    """Decode translation file bytes from common source encodings."""
    # Use "utf-16" when a BOM is present: "utf-16-le"/"utf-16-be" do not strip FF FE /
    # FE FF and would decode them as U+FEFF, duplicating the BOM on re-encode.
    if raw.startswith(BOM_UTF16_LE) or raw.startswith(BOM_UTF16_BE):
        return raw.decode('utf-16')
    if raw.startswith(BOM_UTF8):
        return raw.decode('utf-8-sig')
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        pass
    try:
        return raw.decode('cp1252')
    except UnicodeDecodeError:
        pass
    return raw.decode('latin-1')


def _encode_utf16_le_with_bom(text: str) -> bytes:
    """Encode text as UTF-16 LE with BOM (Skyrim MCM expectation)."""
    # Normalize stray BOM characters (e.g. from mis-decoded sources).
    if text.startswith('\ufeff'):
        text = text.lstrip('\ufeff')
    return BOM_UTF16_LE + text.encode('utf-16-le')


def fix_translation_files(translations_dir='papyrus/Interface/translations'):
    """
    Scan translation files and normalize each to UTF-16 LE with BOM.

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
            
            text = _decode_translation_bytes(content)
            normalized = _encode_utf16_le_with_bom(text)
            if normalized == content:
                files_already_ok.append(file_path.name)
            else:
                with open(file_path, 'wb') as f:
                    f.write(normalized)
                files_fixed.append(file_path.name)
                
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
        print(f"[FIXED] {len(files_fixed)} file(s) converted to UTF-16 LE with BOM:")
        for filename in sorted(files_fixed):
            print(f"  - {filename}")
        print()
    
    if files_already_ok:
        print(f"[OK] {len(files_already_ok)} file(s) already UTF-16 LE with BOM:")
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
