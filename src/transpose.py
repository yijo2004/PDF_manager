import sys
import re
import fitz  # PyMuPDF

# Standard 12-tone chromatic sequence (sharps)
CHROMATIC_SHARPS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Enharmonic equivalents for flats
ENHARMONIC = {
    "Cb": "B",
    "Db": "C#",
    "Eb": "D#",
    "Fb": "E",
    "Gb": "F#",
    "Ab": "G#",
    "Bb": "A#",
    "E#": "F",
    "B#": "C",
}

ALL_KEYS = set(CHROMATIC_SHARPS) | set(ENHARMONIC.keys())


def normalize_key(key):
    if key is None:
        raise ValueError("Key is required")
    key = key.strip().replace("maj", "").upper()
    if key in ENHARMONIC:
        return ENHARMONIC[key]
    if key in CHROMATIC_SHARPS:
        return key
    raise ValueError(f"Invalid key name: {key}")


def calculate_steps(start_key, end_key):
    start = normalize_key(start_key)
    end = normalize_key(end_key)
    start_idx = CHROMATIC_SHARPS.index(start)
    end_idx = CHROMATIC_SHARPS.index(end)
    return (end_idx - start_idx) % 12


def note_to_index(root):
    root = root.strip()
    if not root:
        return None
    m = re.match(r"^([A-Ga-g])([#b]{0,2})$", root)
    if not m:
        return None
    base, accidentals = m.groups()
    base = base.upper()
    if base not in CHROMATIC_SHARPS and base in ENHARMONIC:
        base = ENHARMONIC[base]
    idx = CHROMATIC_SHARPS.index(base)
    for ch in accidentals:
        if ch == "#":
            idx += 1
        elif ch == "b":
            idx -= 1
    return idx % 12


def index_to_note(index):
    return CHROMATIC_SHARPS[index % 12]


def transpose_root_note(root, steps):
    root = root.strip()
    if not root:
        return root
    m = re.match(r"^([A-Ga-g])([#b]{0,2})$", root)
    if not m:
        return root

    base, accidentals = m.groups()
    key = base.upper() + accidentals
    if key in ENHARMONIC:
        key = ENHARMONIC[key]

    idx = note_to_index(key)
    if idx is None:
        return root
    return index_to_note(idx + steps)


def transpose_chord(chord, steps):
    slash_part = None
    if "/" in chord:
        main, slash_part = chord.split("/", 1)
    else:
        main = chord

    match = re.match(r"^([A-G](?:#|b){0,2})(.*)$", main)
    if not match:
        return chord

    root, remainder = match.groups()
    new_root = transpose_root_note(root, steps)
    transposed = new_root + remainder

    if slash_part:
        bass_match = re.match(r"^([A-G](?:#|b){0,2})(.*)$", slash_part)
        if bass_match:
            bass_root, bass_rest = bass_match.groups()
            transposed_bass = transpose_root_note(bass_root, steps) + bass_rest
        else:
            transposed_bass = slash_part
        transposed = f"{transposed}/{transposed_bass}"

    return transposed


def transpose_text(text, steps):
    chord_pattern = re.compile(
        r"(?<![\w/])([A-G](?:#|b){0,2}(?:maj7|m7|m6|m9|m11|m13|dim|aug|sus2|sus4|sus|add\d*|M7|M9|M11|M13|maj|m|7|9|11|13|6|5|2|4)?(?:/[A-G](?:#|b){0,2})?)(?![\w/])",
        re.IGNORECASE,
    )

    def replacer(match):
        chord = match.group(1)
        return transpose_chord(chord, steps)

    return chord_pattern.sub(replacer, text)


def extract_key_from_text(text):
    m = re.search(r"\bKey[:\s]+([A-G](?:#|b)?)\b", text, re.I)
    if m:
        return m.group(1)
    m = re.search(r"\b([A-G](?:#|b)?)\s+major\b", text, re.I)
    if m:
        return m.group(1)
    m = re.search(r"\b([A-G](?:#|b)?)\s+minor\b", text, re.I)
    if m:
        return m.group(1)
    return None


def transpose_pdf(input_pdf, output_pdf, target_key, current_key=None):
    try:
        src = fitz.open(input_pdf)
    except Exception as e:
        raise RuntimeError(f"Error opening PDF '{input_pdf}': {e}")

    full_text = "".join(page.get_text() for page in src)
    detected_key = extract_key_from_text(full_text)

    if current_key is None:
        if detected_key:
            current_key = detected_key
            print(f"Detected key in PDF: {current_key}")
        else:
            raise ValueError("Current key not provided and could not be detected from PDF")

    steps = calculate_steps(current_key, target_key)

    dst = fitz.open()
    dst.insert_pdf(src)

    for page_number in range(len(dst)):
        page = dst[page_number]
        original = src[page_number]

        text_dict = original.get_text("dict")
        for block in text_dict.get("blocks", []):
            for line in block.get("lines", []):
                for span in line.get("spans", []):
                    orig_text = span.get("text", "")
                    transposed_text = transpose_text(orig_text, steps)
                    if transposed_text != orig_text:
                        x0, y0, x1, y1 = span.get("bbox", (0, 0, 0, 0))
                        font_size = span.get("size", 12)
                        font_name = span.get("font", "helv")

                        # erase the original chord text from this span area
                        rect = fitz.Rect(x0, y0, x1, y1)
                        page.draw_rect(rect, color=(1, 1, 1), fill=(1, 1, 1))

                        # keep same baseline approximation
                        baseline = y1 - font_size * 0.2

                        bold_font_candidates = []
                        if "bold" in font_name.lower():
                            bold_font_candidates.append(font_name)
                        # common built-in bold names
                        bold_font_candidates += [
                            "Helvetica-Bold",
                            "Times-Bold",
                            "Courier-Bold",
                            "helvB",
                            "helv",
                        ]

                        drawn = False
                        for candidate in bold_font_candidates:
                            try:
                                page.insert_text(
                                    (x0, baseline),
                                    transposed_text,
                                    fontsize=font_size,
                                    fontname=candidate,
                                    color=(0, 0, 0),
                                )
                                drawn = True
                                break
                            except Exception:
                                continue

                        if not drawn:
                            # fallback: 2x draw using helv to simulate bold, but with small offset
                            for dx, dy in [(0, 0), (0.15, 0), (0, 0.15), (0.15, 0.15)]:
                                try:
                                    page.insert_text(
                                        (x0 + dx, baseline + dy),
                                        transposed_text,
                                        fontsize=font_size,
                                        fontname="helv",
                                        color=(0, 0, 0),
                                    )
                                except Exception:
                                    pass

    dst.save(output_pdf)
    dst.close()
    src.close()
    print(f"Saved transposed PDF to: {output_pdf}")


if __name__ == "__main__":
    if not (4 <= len(sys.argv) <= 5):
        print("Usage: python3 transpose.py <input.pdf> <output.pdf> <current_key> <target_key>")
        print("Or: python3 transpose.py <input.pdf> <output.pdf> <target_key>  # auto-detect current key from PDF")
        sys.exit(1)

    input_pdf = sys.argv[1]
    output_pdf = sys.argv[2]

    if len(sys.argv) == 5:
        current_key = sys.argv[3]
        target_key = sys.argv[4]
    else:
        current_key = None
        target_key = sys.argv[3]

    try:
        transpose_pdf(input_pdf, output_pdf, target_key, current_key)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
