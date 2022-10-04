# addb - A simple plaintext sql interpreter

This implements a simple SQL:ish interpreter. It's more of a toy/proof-of-concept doing an SQL-style interpreter from first principles. It takes the same stupid approach to interpretation as early BASICs did: It re-parses and interprets the code *every* time. Is it efficient? No. Was it easy to write? Yes!

## features

- plain-text file querying
- NULL (as in undefined variable/column)
- double as only numeric type
- string as string type
- $lineno of generated out-lines
- generic iterator/generator in from-clause
- built in int iterator
- no optimization
- order of data in kept in result
- (only) nested loop evaluation

## supported

Integer iterator:

    select i, i*i from int(1,10) i

CSV file querying:

    select a,b,c from foo.csv foo

## TODO/IDEAS:

- tab join tab using(c, ...)
- Query 22 Mb external URL

      SELECT ...
      FROM  https://raw.githubusercontent.com/megagonlabs/HappyDB/master/happydb/data/cleaned_hm.csv AS happy
      WHERE ...

      download, cache

      Possibly an specific

      IMPORT TABLE happy FROM https://...


