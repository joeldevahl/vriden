Unit:Using("application")
Unit:Using("graphics")

function Unit.Init(self)
	self.executable = true
	self.targetname = "vriden"
end

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.dll.libs:Add("user32")
		self.settings.link.libs:Add("user32")
		self.settings.link.flags:Add("/SUBSYSTEM:WINDOWS")
	end

	local common_src = Collect(self.path .. "/*.cpp")
	local common_obj = Compile(self.settings, common_src)

	local job_src = Collect(self.path .. "/jobs/*.cpp")
	local job_obj = Compile(self.settings, job_src)
	local job_dll = SharedLibrary(self.settings, self.targetname .. "_jobs", job_obj, common_obj)
	self:AddProduct(job_dll)

	local bin = Link(self.settings, self.targetname, common_obj)
	self:AddProduct(bin)
end
