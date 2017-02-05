Unit:Using("foundation")

function Unit.Patch(self, other_unit)
	if other_unit["ignore_application_lib"] then
		DefaultPatchHeaders(self, other_unit)
	else
		DefaultPatch(self, other_unit)
		if target.family == "windows" then
			other_unit.settings.link.libs:Add("user32")
			other_unit.settings.link.flags:Add("/SUBSYSTEM:WINDOWS")
		elseif target.platform == "osx" then
			other_unit.settings.link.frameworks:Add("Cocoa")
			other_unit.settings.link.frameworks:Add("QuartzCore")

			-- TODO(joel): hack for making jobs link for now
			other_unit.settings.dll.frameworks:Add("Cocoa")
			other_unit.settings.dll.frameworks:Add("QuartzCore")
		end
	end
end
