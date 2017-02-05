Unit:Using("catch")
Unit:Using("application")

function Unit.Init(self)
	self.executable = true
	self.targetname = "vriden"
end


function Unit.Patch(self, other_unit)
	self:ApplyConfig(other_unit)
	self:PatchHeaders(other_unit)
	other_unit.settings.cc.includes:Add(self.path)
end

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.dll.libs:Add("user32")
	end

	local common_src = Collect(self.path .. "/*.cpp")
	local common_obj = Compile(self.settings, common_src)

	-- use integrated tests for now
	local test_src = Collect(self.path .. "tests/*.cpp")
	local test_obj = Compile(self.settings, test_src)

	local job_src = Collect(self.path .. "/jobs/*.cpp")
	local job_obj = Compile(self.settings, job_src)
	local job_dll = SharedLibrary(self.settings, self.targetname .. "_jobs", job_obj, common_obj)
	self:AddProduct(job_dll)

	local bin = Link(self.settings, self.targetname, common_obj, test_obj)
	self:AddProduct(bin)
end
