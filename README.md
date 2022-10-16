# addb - A simple plaintext sql interpreter

This implements a simple SQL:ish interpreter. It's more of a toy/proof-of-concept doing an SQL-style interpreter from first principles. It takes the same stupid approach to interpretation as early BASICs did: It re-parses and interprets the code *every* time. Is it efficient? No. Was it easy to write? Yes! So far...

## GOALS

- "minimal" in code/complexity (<1000 lines code)
- fast hack for fun
- "not for professional use"
- limited sql
	SELECT count(money) xxx AS x, ...
	FROM fil.csv AS fil
	JOIN foo.csv ON fil.id=foo.id
	WHERE ...
	AND ...
	OR NOT ...
	GROUP BY 1,2
- sql *INTERPRETER* (means no internal structures/parse-tree/interpreter internal representation)
- parse text files (csv, tab, plain, json, xml)
- limited aggregation (full/group by on sorted data)
- some variant of merge-join (on sorted data)

## NON-goals
- full standard SQL (we go for subset)
- efficency
- tokenizer/parser/parse-tree
- optimizer
- more datatypes
- ODBC
- JDBC no fxxing way!
- stored prepared queries

## Current Features
- row by row processing
- plain-text CSV/TAB-file querying
- NULL (as in undefined variable/column)
- double as only numeric type (print %lg)
- string as string type
- $lineno of generated out-lines
- generic iterator/generator in from-clause
- built in int iterator
- no optimization
- order of data in kept in result
- (only?) nested loop evaluation
- WHERE ... AND ... OR NOT ...
- undefined variables/colum names used are considered null
- NULL is always null if not set, LOL ("feature")
- variable stats data used for aggregates
- COUNT(), SUM(), MIN(), MAX(), AVG(), DEV(), 
- aggregators only work on variables, not computed values:
       doesn't work (currently):

         select sum(1) from int(1,10) i

       workaround:
       
         select 1 as foo, sumfoo) fror int(1,10) i


## functions
- mod div
- ascii char lower upper
- count sum min max avg stdev

## TODO:
- log queries/statistics
- val with altname/num/table.col
- upper/lower-case for "sql"
- where not/and/or...
- functions in expressions
  - date/time functions (on strings)
  - json/xml extract val f string
  - more math?
- MAX/MIN/COUNT/AVG/VAR on whole file/NO groupby but with WHERE
- xml querying
- json querying
- flatfile querying
- APPEND (not UPDATE?) == SELECT ... INTO ... ?
- BEGIN...END transactional over several files?
- indix files=="create view" (auto-invalidate on update)
- consider connecting with user pipes and external programs like sql_orderby?
- tab JOIN tab using(c, ...)
- Query 22 Mb external URL
      SELECT ...
      FROM  https://raw.githubusercontent.com/megagonlabs/HappyDB/master/happydb/data/cleaned_hm.csv AS happy
      download, cache

      Possibly an specific

      IMPORT TABLE happy FROM https://...

## NOT todo

- optimizer
- row values/tuples
- more datatypes (just keep as string)
- Ordering and/or Aggregation with GROUP BY. Basically this would either have to use memory or temporary files. Not such good idea. Maybe generate set and then call unix "sort" with right arguments/types as ORDER BY, then could do GROUP BY.
- GROUP BY (unless sorted input?)
- ORDER BY

## supported

Calculations

    select 1+3*4   => 13
    
Where clause

    select 42 where 0=3*4 or 1=1 and 2=2
    
Integer iterator (EXTENSION!):

    select i, i*i from int(1,10) i where i*i>10

CSV file querying (EXTENSION!):

    select a,b,c from foo.csv foo

Function calling

    select 

