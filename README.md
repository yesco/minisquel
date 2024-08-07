# MiniSQueL - a minimalistic plaintext sql interpreter

This implements a simple SQL:ish interpreter in 1008 lines of C!

It's more of a toy/proof-of-concept doing an SQL-style interpreter from first principles. It takes the same naive/simple approach to interpretation as early BASICs did: It re-parses and interprets the source code *every* time. Is it efficient? No. Was it easy to write? Yes! So far...

Update: experimental much faster parse.c that compiles to ObjectLog interpreted by objectlog.c; run sql using ./olsql "select('foo', 42, 'bar');"

## GOALS

- "minimal" in code/complexity (~1000 lines code (count by /wcode))
- fast hack for fun
- do the simplest for the moment to add functionality
- "not for professional use" :-D
- no depencies other than linux/posix
- limited sql
```
     [FORMAT (CSV|TAB|BAR)]
     SELECT count(money) xxx AS x, ...
     [FROM fil.csv AS fil
     [JOIN foo.csv AS foo ON fil.id=foo.id]]
     [WHERE ...
     AND ...
     OR NOT ...]
     [GROUP BY 1,2]
     [ORDER BY x DESC]
```
- sql *INTERPRETER* (means no internal structures/parse-tree/interpreter internal representation)
- parse text files (csv, tab, plain, json, xml)
- limited aggregation (full/group by on sorted data)
- some variant of merge-join (on sorted data)

## NON-goals
- NO: full standard SQL (we go for subset)
- NO: high optimial efficency (go download DuckDB instead!)
- NO: tokenizer/parser-tree/internal represenation (yet... lol)
- NO: optimizer
- NO: more datatypes (but funtions on strings: xml/json etc, can add fancy syntax: . -> or even a xpath?)
- NO: ODBC, NO: JDBC - no fxxing way!
- NO: stored procedures P/SQL
- NO: row values/tuples

## Working Examples

Calculations

    select 1+3*4   => 13
    
Naming and using columns (EXTENSION!)

    select 42 as ft, ft+7, ft*ft

complex where clause

    select 42 where 0=3*4 or 1=1 and 2=2
    
Integer iterator (EXTENSION!):

    select i, i*i from int(1,10) i where i*i>10

CSV file querying (EXTENSION!):

    select a,b,c from foo.csv foo

Function calling

    select "abba" as f, char(ascii(upper(f)))

Joins!

    select int.a, b, a*int.b
    from int(1,10) a, int(1,10) b
    where a=b
    
    select int.a, b, a*int.b from int(1,10) a, int(1,10) b where a=b
    
You can even get optimized index lookup join, but you have to use the "JOIN" keyword to force the second table (?) to be stored in memory and then sorted.

External program quering (EXTENSION!)

     select name from \"ls -1 |\"(name) file\n\

This runs the command ending with a pipe character '|'. The output is parsed as CSV and used for the query. The columns (1 in this case) are named (name).

If this is put in a script it can be queried like this:

     select name from "./sql --batch --init Test/files.sql |"

Here is another short-cut (EXTENDED!)

     File: one2ten.sql
     ---
     select i from int(1,10) i
     ---

     select i from one2ten.sql o2t

Basically, we're saying that a sql script (with one statement) *IS* a table!

This means we have limited subqueries at least in the FROM clause.

Select column names by 'like'-style expression:

     select [foo.*] from foo foo order by 1

ORDER BY colnum [ ASC[ENDING] | DESC[ENDING] ]

     select -i, i from int(1,10) ORDER BY 1 ASC

     select -i, i from int(1,10) ORDER BY 2 DESCENDING

or happen to work...

     select -i, i from int(1,10) ORDER BY -2



### CSV input/output format

The CSV-parser is handwritten and it's small, lean, extreemly fast. It handles quoted strings, commas inside strings, backslash quoted, double "-quoted, or using '-quoted strings.

It's silly flexible, in that it looks a the first line and tries to figure out what delimited to use. The default assumption is comma, but it uses heuristics to try all of the following: , ; : \t | or plain spaced -- whatever is most occuring.

Using the FORMAT keyword you can choose between CSV TAB BAR output formats.

Also, using --browse, as explained below, you can view the results. No longer have to suffering through endless database output scrolling by on the screen. (pretty useless unless piped/stored)

### Pretty Browsing

Only one column for now. This stores the result set in a main memory results table. By giving the --browse option, you can. GUI/browse a pretty printed variant of the table. More features coming!

     ./sql --browse select -i, i from int(1,10) 


### Schema

Simple shell commands to show current tables (.sql-files) and views (.csv).

     UNIX> ./tables

A single table's columns are shown by:

     UNIX> ./describe foo.csv


### Create Index

It's a dummy for now, it uses a special in-memory 12 byte structure. It's being replaced by the new in-memory double value table structure.

     SQL> create index ix on foo.csv(a)

     UNIX> ./index foo.csv a

But it's not used.


## Performance?

- actually, not too bad!
- 22MB of csv w 108K records take < 1s to scan! (competetive with DuckDB!)
- 1000x1000 iterations <1s (1.4M "where"-evals)
- opening/close same file 100,000 times (in nested loop) almost no overhead! (modern computers/mobile phones use SSD flash, incredible fast)


## Current Features - DONE

- last column in fancy mode not truncate, also for header...
- truncate print strings to 7 cols showing * at end if truncated, only for "formatted" not "csv" (TODO: how to disable?)
- select [foo.*] from foo foo order by 1
- less memory leaks! LOL
- LIKE and ILIKE operators added
- operators
      !> < <= = ~= == <> != => > !<
      like ilike
      and or not

      NOTE: = and ~= are equals 1e-9 relative difference.
            or for strings: case indiffernt =
      NOTE: == is c == (for double)
- rudimentary automatic build of in-memory index 16B/entry
- LIMITED: tab JOIN tab ON col - speedup is from 30h to 5s! using index on cross-join which becomes nested loop index lookup. 2s is scanning the two (same) tables and building the index ("zero" time)
- row by row processing
- cross-product join (slow)
- refer to columns by col or table.col
- table names can be quoted (getsymbol)
- plain-text CSV/TAB-file or even passwd-file querying
- SET @varible = 3+4*7
- variables can/should be '@var'
- undefined/not present variables are NULL
- NULL is always null if not set, LOL ("feature")
- double as only numeric type (print %lg)
- string as "other type"
- $lineno of generated out-lines
- generic iterator/generator in from-clause
- built in INT iterator
- no optimization
- order of data in kept in result
- (only?) nested loop evaluation
- WHERE ... AND ... OR NOT ...
- variable stats data used for aggregates:
- COUNT(), SUM(), MIN(), MAX(), AVG(), DEV(), 
- log queries/statistics
- aggregators only work on variables, not computed values:
       doesn't work (currently):

         select sum(1) from int(1,10) i

       workaround:
       
         select 1 as foo, sumfoo) fror int(1,10) i


## functions

- mod div
- ascii char concat ilike left like lower right timestamp upper
- count sum min max avg stdev
- type (!)


## ./minisquel - "help"

```
$ ./sql --help

Usage: ./sql ( [OPTIONS] [SQL] ) ... 

[OPTIONS]
--batch
	(== --csv --no-echo --no-stats --no-interactive)
--csv
--echo
--force
--format=csv|bar|tab	(tab is default)
--init FILENAME
	Loads and runs SQL from file.
--interactive | -t
--security
	Disables potentially dangerious operations.
	popen: select name from "ls -1 |"(name) file
--stats
--verbose | -v
--verbose

[SQL]	'select 42'
	'select "foo"'

	If queries are run command line, it'll exit after.
	To change; add--interactive.


Unknown option: --help
Error: Unkonwn option
```


## TODO:

- columns default to only 7 chars, make it more adoptable? or default to --browse (and make it work?)
- make alias table name optional, but maybe need to test for "uniqueness" (self-joins) select [foo.*] from foo foo order by 1
- allow = and operators inside SELECT, maybe:
- maybe: need a boolean type? (duckdb has, sqlite uses 0, 1)
- tab JOIN tab USING(c, ...)
- CREATE TABLE xxx AS SELECT ...
- LIKE/REGEXP
- CREATE FUNCTION Bigtable UDF:s - https://cloud.google.com/bigquery/docs/reference/standard-sql/user-defined-functions
- val with altname/num/table.col
- where not/and/or...
- functions in expressions
  - date/time functions (on strings)
  - json/xml extract val from string
  - more math?
- xml querying
- json querying
- yaml querying
- flatfile querying
- APPEND (not UPDATE?) == SELECT ... INTO ... ?
- BEGIN...END transactional over several files?
- index files=="create view" (auto-invalidate on update)
- consider connecting with user pipes and external programs like sql_orderby? or for nested queries, at least in "from"
- Query 22 Mb external URL
      SELECT ...
      FROM  https://raw.githubusercontent.com/megagonlabs/HappyDB/master/happydb/data/cleaned_hm.csv AS happy
      download, cache

      Possibly an specific

      IMPORT TABLE happy FROM https://...

