Unit:Using("foundation")

-- compilers, find a way to make this non transitive
-- and make sure only the compilers actually link this
Unit:Using("getopt")
Unit:Using("dl")
Unit:Using("assimp")

function Unit.AddTools(self)

	local function AddCompiler(name, extension)
		AddTool(function (settings)
			settings[name] = {}
			settings[name].exe = GetHostBinary(name)

			settings.compile.mappings[extension] = function (settings, infile)
				local outfile = PathJoin(target.outdir, infile)
				local compiler = settings[name]
				local exe = compiler.exe
				local cmd = exe .. " -o " .. outfile .. " " .. infile

				AddJob(outfile, name .. " " .. infile, cmd)
				AddDependency(outfile, exe)
				AddDependency(outfile, infile)

				for line in io.lines(infile) do
					local dep_type, file = string.match(line, "//%s*dep%s+(.+)%s+(.+)")
					if dep_type and file then
						if dep_type == "source" then
							AddDependency(outfile, file)
						else
							-- TODO: transform source to binary version
						end
					end
				end

				return outfile
			end
		end)
	end

	AddCompiler("materialc", "material")
	AddCompiler("meshc", "mesh")
	AddCompiler("shaderc", "shader")
	AddCompiler("texturec", "texture")

	AddTool(function (settings)
		settings.vertex_layoutc = {}
		settings.vertex_layoutc.libname = "units/graphics/types/vertex_layout.tlb"

		settings.compile.mappings["vl"] = function (settings, infile)
			local outfile = PathJoin(target.outdir, PathBase(infile)) .. ".vl"
			local libname = PathJoin(target.outdir, settings.vertex_layoutc.libname)
			return DLPack(settings, infile, outfile, libname)
		end
	end)
end

function Unit.Patch(self, other_unit)
	self:DefaultPatch(other_unit)
	if target.family == "windows" then
		other_unit.settings.link.libs:Add("d3d12")
		other_unit.settings.link.libs:Add("dxguid")
		other_unit.settings.link.libs:Add("dxgi")

		other_unit.settings.dll.libs:Add("d3d12")
		other_unit.settings.dll.libs:Add("dxguid")
		other_unit.settings.dll.libs:Add("dxgi")
	end
end

function Unit.BuildTarget(self)
	local typelib_src = Collect(self.path .. "/types/*.tld")
	local typelib = Compile(self.settings, typelib_src);
	self:AddProduct(typelib)
end

function Unit.Build(self)
	local graphics_src = {
		PathJoin(self.path, "src/render_common.cpp"),
		PathJoin(self.path, "src/render_null.cpp"),
		PathJoin(self.path, "src/render_dx12.cpp"),
	}

	local graphics_obj = Compile(self.settings, graphics_src)
	local graphics = StaticLibrary(self.settings, "graphics", graphics_obj)
	self:AddProduct(graphics)

	local materialc_obj = Compile(self.settings, PathJoin(self.path, "src/materialc.cpp"))
	local materialc = Link(self.settings, "materialc", materialc_obj)
	self:AddProduct(materialc)

	local texturec_obj = Compile(self.settings, PathJoin(self.path, "src/texturec.cpp"))
	local texturec = Link(self.settings, "texturec", texturec_obj)
	self:AddProduct(texturec)
	
	local shaderc_settings = self.settings:Copy()
	if target.family == "windows" then
		shaderc_settings.link.libs:Add("d3dcompiler")
	end
	local shaderc_obj = Compile(shaderc_settings, PathJoin(self.path, "src/shaderc.cpp"), PathJoin(self.path, "src/shaderc_dx12.cpp"))
	local shaderc = Link(shaderc_settings, "shaderc", shaderc_obj)
	self:AddProduct(shaderc)

	local meshc_obj = Compile(self.settings, PathJoin(self.path, "src/meshc.cpp"))
	local meshc = Link(self.settings, "meshc", meshc_obj)
	self:AddProduct(meshc)
end
