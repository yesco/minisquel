# NOTE:
#   1. single out parameter defined
#      first! lol, input last
#   2. select out, in1, in2, ...
#   3. HAHA
#
#   4. Single return value not need CALL!
#      Maybe could inline or make "out"
#      cut and copy valuies back!

# but it works!

./olrun \
  'create function double(b,a) as select a+a,a' \
  'select i,double(i) from int(1,10) i' 
