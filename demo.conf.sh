set numbers
set numbered
set number
set suffix
set compressed

@HERE@/DEMO1	size=200k,
			class="daily",
			save 10 in @HERE@/OLD,
			signal="echo \"hello, world\"";
			signal="echo Hi There!";

@HERE@/DEMO2	size(200k)
			class("weekly")
			truncate
			prefix
			dots
 			save(3) in (@HERE@/OLD)
			signal("echo \"hello, world\"")
			signal("echo hello,sailor")

@HERE@/DEMO3	size=200k
			save=3 in @HERE@/OLD
			uncompressed
			touch=644
			signal="echo \"hello, world\""
			signal="echo \"So, who are you?\""

@HERE@/DEMO4	size 200k
			save 3 in @HERE@/OLD

@HERE@/DEMO5	save in @HERE@/OLD every week
