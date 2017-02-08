Unit.config_set = { "optimizations" }
Unit:Using("zlib")

function Unit.Init(self)
	self:DefaultInit()

	table.insert(self.source_dirs, "contrib/ConvertUTF")
	table.insert(self.source_dirs, "contrib/irrXML")
	table.insert(self.source_dirs, "contrib/unzip")
	table.insert(self.source_dirs, "contrib/poly2tri/poly2tri/common")
	table.insert(self.source_dirs, "contrib/poly2tri/poly2tri/sweep")
	table.insert(self.source_dirs, "contrib/clipper")
end

function Unit.Build(self)
	self.settings.cc.includes:Add(PathJoin(self.path, "src/BoostWorkaround"))
	self.settings.cc.includes:Add(self.path)
	self.settings.cc.defines:Add("ASSIMP_BUILD_BOOST_WORKAROUND")
	self.settings.cc.defines:Add("ASSIMP_BUILD_NO_OWN_ZLIB")
	if target.family == "windows" then
		self.settings.cc.defines:Add("_SCL_SECURE_NO_WARNINGS") -- Disable some warnings
		self.settings.cc.flags:Add("/wd4577") -- Disable warning about exception handler
		self.settings.cc.flags:Add("/wd4530") -- Disable warning about exception handler
	end
	DefaultBuild(self)
end
