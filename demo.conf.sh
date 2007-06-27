@HERE@/DEMO1	size=200k,
			class="daily",
			save 3 in @HERE@/OLD,
			signal="echo \"hello, world\"";

@HERE@/DEMO2	size(200k)
			class("weekly")
		#	save(3) in(@HERE@/OLD)
			truncate
			signal("echo \"hello, world\"")

@HERE@/DEMO3	size=200k
			save=3 in @HERE@/OLD
			touch=644
			signal="echo \"hello, world\""

@HERE@/DEMO4	size 200k
			save 3 in @HERE@/OLD

@HERE@/DEMO5	save in @HERE@/OLD every week
