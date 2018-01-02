Unit:Using("foundation")

function Unit.Build(self)
	local game_src = {
		PathJoin(self.path, "src/ecs.cpp"),
	}

	local game_obj = Compile(self.settings, game_src)
	local game = StaticLibrary(self.settings, "game", game_obj)
	self:AddProduct(game)
end
