Unit:Using("getopt")
Unit:Using("hexify")
Unit.config_set = { "optimizations" }

function Unit.Init(self)
	self.executable = true
	self.static_library = true
	self.targetname = "dl"
end

function DLPack(settings, infile, outfile, libname)
	local dl_pack = settings.dl_pack
	local cmd = dl_pack.exe .. " -L " .. target.outdir .. " -l " .. libname .. " -o " .. outfile .. " " .. infile
	AddJob(outfile, "dl_pack " .. infile, cmd,
		--deps
		infile, dl_pack.exe, libname)
	return outfile
end

function Unit.AddTools(self)
	AddTool(function (settings)
		settings.dl_tlc = {}
		settings.dl_tlc.exe = GetHostBinary("dl_tlc")

		settings.dl_pack = {}
		settings.dl_pack.exe = GetHostBinary("dl_pack")

		settings.compile.mappings["tld"] = function (settings, infile)
			local outfile_base = PathJoin(target.outdir, PathBase(infile))
			local outfile_h = outfile_base .. ".h"
			local outfile_tlb = outfile_base .. ".tlb"
			local dl_tlc = settings.dl_tlc

			local cmd_h = dl_tlc.exe .. " -c -o " .. outfile_h .. " " .. infile
			AddJob(outfile_h, "tlc " .. infile,
				--deps
				cmd_h, dl_tlc.exe, infile)

			local cmd_tlb = dl_tlc.exe .. " -o " .. outfile_tlb .. " " .. infile
			AddJob(outfile_tlb, "tlc " .. infile,
				--deps
				cmd_tlb, dl_tlc.exe, infile)

			return { outfile_tlb, Hexify(settings, outfile_tlb) }
		end
	end)
end

function Unit.Build(self)
		local lib_src = Collect(PathJoin(self.path, "src/*.cpp"))
		local lib_obj = Compile(self.settings, lib_src)
		local lib = StaticLibrary(self.settings, self.targetname, lib_obj)
		self:AddProduct(lib)

		self.settings.link.libs:Add(self.targetname .. self.settings.config_ext)
		local dl_pack_src = Collect(PathJoin(self.path, "src/dl_pack/*.cpp"))
		local dl_pack_obj = Compile(self.settings, dl_pack_src)
		local dl_pack = Link(self.settings, "dl_pack", dl_pack_obj)
		self:AddProduct(dl_pack)

		self.settings.link.libs:Add(self.targetname .. self.settings.config_ext)
		local dl_tlc_src = Collect(PathJoin(self.path, "src/dl_tlc/*.cpp"))
		local dl_tlc_obj = Compile(self.settings, dl_tlc_src)
		local dl_tlc = Link(self.settings, "dl_tlc", dl_tlc_obj)
		self:AddProduct(dl_tlc)
end
