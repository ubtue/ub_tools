import spacy


def GetHighestLabel(cats):
    return list(sorted(cats, key=cats.get, reverse=True))[0]

nlp = spacy.load("../training/output1/model-best")
text = ""
print("type : 'quit' to exit")
while text != "quit":
    text = input("Please enter example input: ")
    doc = nlp(text)
    print(GetHighestLabel(doc.cats).upper())
    print(doc.cats)
           
