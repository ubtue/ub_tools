-- Control numbers are saved as concatenated lists of individual numbers delimited by the '|' character

CREATE TABLE normalised_titles (
    title TEXT NOT NULL PRIMARY KEY,
    control_numbers TEXT NOT NULL,
    UNIQUE (title)
);
CREATE INDEX normalised_titles_index ON normalised_titles(title);

CREATE TABLE normalised_authors (
    author TEXT NOT NULL PRIMARY KEY,
    control_numbers TEXT NOT NULL,
    UNIQUE (author)
);
CREATE INDEX normalised_authors_index ON normalised_authors(author);

CREATE TABLE publication_year (
    year TEXT NOT NULL PRIMARY KEY,
    control_numbers TEXT NOT NULL,
    UNIQUE (year)
);
CREATE INDEX publication_year_index ON publication_year(year);
