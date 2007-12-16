set suffix
set number
set numbered
set dots
set compressed

@HERE@/DEMO1	size=200k,
			class="daily",
			save 10 in @HERE@/OLD,
			signal="echo \"hello, world\"";
			signal="echo Hi There!";

@HERE@/DEMO2	size(200k)
			class("weekly")
			truncate
 			save(3) in (@HERE@/OLD)
			signal("echo \"hello, world\"")
			signal("echo hello,sailor")

@HERE@/DEMO3	size=200k
			save=3 in @HERE@/OLD
			touch=644
			signal="echo \"hello, world\""
			signal="echo \"So, who are you?\""

@HERE@/DEMO4	size 200k
			save 3 in @HERE@/OLD

@HERE@/DEMO5	save in @HERE@/OLD every week
