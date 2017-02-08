Unit.config_set = { "optimizations" }

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.cc.flags:Add( "/TP" ) -- forcing c++ compile on windows =/
	end
	self:DefaultBuild()
end
