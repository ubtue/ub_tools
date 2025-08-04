import sys
import fitz  # PyMuPDF
import os

def pdf_to_markdown(pdf_path, md_path):
    doc = fitz.open(pdf_path)
    with open(md_path, 'w', encoding='utf-8') as md_file:
        for page_num in range(len(doc)):
            page = doc.load_page(page_num)
            text = page.get_text()
            # Simple markdown conversion: each page as a header
            md_file.write(f"# Page {page_num + 1}\n\n")
            md_file.write(text)
            md_file.write('\n\n---\n\n')  # Page separator

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {os.path.basename(__file__)} <input.pdf>")
        sys.exit(1)
    pdf_path = sys.argv[1]
    if not os.path.isfile(pdf_path):
        print(f"File not found: {pdf_path}")
        sys.exit(1)
    md_path = os.path.splitext(pdf_path)[0] + ".md"
    pdf_to_markdown(pdf_path, md_path)
