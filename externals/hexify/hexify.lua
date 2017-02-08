function Hexify(settings, ...)
	local hexify = settings.hexify

	local outfiles = {}
	for infile in TableWalk({...}) do
		local outfile = GetOutputNameWithoutExt(infile) .. "." .. PathFileExt(infile) .. ".hex"
		local cmd = hexify.exe .. " " .. infile .. " > " .. outfile
		AddJob(outfile, "hexify " .. infile, cmd, infile, hexify.exe)
		table.insert(outfiles, outfile)
	end

	return outfiles
end

function Unit.AddTools(self)
	AddTool(function (settings)
		settings.hexify = {}
		settings.hexify.exe = GetHostBinary("hexify")
	end)
end

function Unit.Init(self)
	self:DefaultInit()
	self.executable = true
	self.static_library = false
end
