import sys
import fitz  # PyMuPDF
import os

def page_contains_preview(page):
    text = page.get_text()
    for line in text.splitlines():
        if line.strip() == "Preview":
            return True
    return False

def pdf_to_markdown(pdf_path, md_path):
    doc = fitz.open(pdf_path)

    first_page = doc.load_page(0)

    if page_contains_preview(first_page):
        pages_to_convert = [1, 2, len(doc) - 1]
    else:
        pages_to_convert = [0, 1, len(doc) - 1]

    pages_to_convert = sorted(set(p for p in pages_to_convert if 0 <= p < len(doc)))

    with open(md_path, 'w', encoding='utf-8') as md_file:
        for p in pages_to_convert:
            page = doc.load_page(p)
            text = page.get_text()
            md_file.write(f"# Page {p}\n\n")
            md_file.write(text)
            md_file.write('\n\n---\n\n')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: python {os.path.basename(__file__)} <input.pdf> <output.md>")
        sys.exit(1)

    pdf_path = sys.argv[1]
    md_path = sys.argv[2]

    if not os.path.isfile(pdf_path):
        print(f"File not found: {pdf_path}")
        sys.exit(1)

    pdf_to_markdown(pdf_path, md_path)
