function Unit.Init(self)
	self.executable = true
	self.targetname = "vriden"
end

function Unit.Build(self)
	if target.family == "windows" then
		self.settings.link.libs:Add("user32")
		self.settings.link.flags:Add("/SUBSYSTEM:WINDOWS")
	end
	local src = Collect(self.path .. "/*.cpp")
	local obj = Compile(self.settings, src)
	local bin = Link(self.settings, self.targetname, obj)
	self:AddProduct(bin)
end
