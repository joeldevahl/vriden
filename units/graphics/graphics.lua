Unit:Using("foundation")

-- compilers, find a way to make this non transitive
Unit:Using("getopt")
Unit:Using("dl")

function Unit.AddTools(self)
	AddTool(function (settings)
		settings.materialc = {}
		settings.materialc.exe = GetHostBinary("materialc")

		settings.compile.mappings["material"] = function (settings, infile)
			local outfile = PathJoin(target.outdir, infile)
			local materialc = settings.materialc
			local exe = materialc.exe
			local cmd = exe .. " -o " .. outfile .. " " .. infile

			AddJob(outfile, "material " .. infile, cmd)
			AddDependency(outfile, materialc.exe)
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
	local graphics = Link(self.settings, "graphics", graphics_obj)
	self:AddProduct(graphics)

	local materialc_obj = Compile(self.settings, PathJoin(self.path, "src/materialc.cpp"))
	local materialc = Link(self.settings, "materialc", materialc_obj)
	self:AddProduct(materialc)
end
