-- Control numbers are saved as a single text blob consisting of individual numbers concatenated together with the '|' character.
-- Individual (control) numbers are additionally padded to their maximum length in the case of the "publication_year" table.
-- All text fields contain UTF-8 strings.

CREATE TABLE normalised_titles (
    title TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (title, control_number)
);
CREATE INDEX normalised_titles_index ON normalised_titles(title);

CREATE TABLE normalised_authors (
    author TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (author, control_number)
);
CREATE INDEX normalised_authors_index ON normalised_authors(author);

CREATE TABLE publication_year (
    year TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (year, control_number)
);
CREATE INDEX publication_year_index ON publication_year(year);

CREATE TABLE doi (
    doi TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (doi, control_number)
);
CREATE INDEX doi_doi_index ON doi(doi);

CREATE TABLE issn (
    issn TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (issn, control_number)
);
CREATE INDEX issn_issn_index ON issn(issn);

CREATE TABLE isbn (
    isbn TEXT NOT NULL,
    control_number TEXT NOT NULL,
    PRIMARY KEY (isbn, control_number)
);
CREATE INDEX isbn_isbn_index ON isbn(isbn);


