local libphx = require('ffi.libphx').lib

function onDef_BSP (t, mt)
  t.Create = function (mesh)
    local e = libphx.Mesh_Validate(mesh)
    if e ~= 0 then
      print('BSP Incoming Mesh Error:')
      libphx.Error_Print(e)
      return nil
    end
    return libphx.BSP_Create(mesh)
  end
end
