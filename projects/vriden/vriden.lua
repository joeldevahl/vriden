Unit:Using("application")
Unit:Using("graphics")
Unit:Using("glm")

function Unit.Init(self)
	self.executable = true
	self.targetname = "vriden"
end

function Unit.BuildTarget(self)
	local data = {
		"data/programs/*.vp",
		"data/programs/*.fp",
		"data/vertex_layouts/*.vl",
		"data/meshes/*.mesh",
		"data/shaders/*.shader",
		"data/textures/*.texture",
		"data/materials/*.material",
	}
	for _,d in ipairs(data) do
		for _,r in pairs(Compile(self.settings, Collect(PathJoin(self.path, d)))) do
			self:AddProduct(r)
		end
	end

	-- TODO: don't write this all the time
	file = io.open(PathJoin(target.outdir, "vriden_precache.txt"), "wb")
	for v in TableWalk(self.target_products[target]) do
		file:write(string.gsub(v, target.outdir .. "/", "") .. "\n")
	end
	file:close()
end

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.dll.libs:Add("user32")
	end

	local common_src = Collect(self.path .. "/src/*.cpp")
	local common_obj = Compile(self.settings, common_src)

	local job_src = Collect(self.path .. "/src/jobs/*.cpp")
	local job_obj = Compile(self.settings, job_src)
	local job_dll = SharedLibrary(self.settings, self.targetname .. "_jobs", job_obj, common_obj)
	self:AddProduct(job_dll)

	local bin = Link(self.settings, self.targetname, common_obj)
	self:AddProduct(bin)
end
