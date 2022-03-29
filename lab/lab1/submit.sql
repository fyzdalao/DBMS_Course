a
select distinct maker from Product where type='laptop' except select distinct maker from Product where type='pc';

b
select distinct maker from Product where maker not in (select maker from Product where type='pc') and maker in (select maker from Product where type='laptop');

c
select distinct maker from Product as A where type='laptop' and not exists (select * from Product as B where B.maker = A.maker and B.type='pc');

d
select P1.model from Printer as P1 join Printer as P2 on (P1.price < P2.price) where P2.model=3002;

e
select model from Printer as P1 where P1.price < (select price from Printer as P2 where P2.model=3002);

f
select model from Printer as P1 where exists (select price from Printer as P2 where P2.model=3002 and P2.price>P1.price);

g
select PC1.model from PC as PC1 left outer join PC as PC2 on (PC1.speed < PC2.speed) where PC2.model is null;

h
select model from PC where speed in (select max(speed) from PC);

i
select model from PC where speed = (select max(speed) from PC);

j
select model from PC where speed >= (select max(speed) from PC);

k
select model from PC as PC1 where not exists (select * from PC as PC2 where PC2.speed > PC1.speed);

l
select distinct A.maker from ((Product join PC using(model)) as A join (Product join PC using(model)) as B on A.speed != B.speed and A.maker = B.maker) join (Product join PC using(model)) as C on A.speed != C.speed and B.speed != C.speed and A.maker = C.maker;

m
select maker from Product join PC using(model) group by maker having count(distinct speed)>=3 ;

n
select maker from (select maker, count(distinct speed) as cnt from Product join PC using(model) group by maker) as A where A.cnt >=3;

o
update PC set price = price*0.90 where model = any (select model from Product where maker = 'A');

p
update PC set price = price*0.90 where model in (select model from Product where maker = 'A');

q
update PC set price = price*0.90 where exists(select * from Product where maker = 'A' and model = PC.model);



