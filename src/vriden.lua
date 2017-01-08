Unit:Using("catch")

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
		self.settings.link.libs:Add("user32")
		self.settings.link.flags:Add("/SUBSYSTEM:WINDOWS")
	end
	local src = Collect(self.path .. "/*.cpp")
	local obj = Compile(self.settings, src)

	-- use integrated tests for now
	local test_src = Collect(self.path .. "tests/*.cpp")
	local test_obj = Compile(self.settings, test_src)

	local bin = Link(self.settings, self.targetname, obj, test_obj)
	self:AddProduct(bin)
end
