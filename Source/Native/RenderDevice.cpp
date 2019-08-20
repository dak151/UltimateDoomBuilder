
#include "Precomp.h"
#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "ShaderManager.h"
#include <stdexcept>

#ifdef WIN32
RenderDevice::RenderDevice(void* hwnd) : Context(hwnd)
#else
RenderDevice::RenderDevice(void* disp, void* window) : Context(disp, window)
#endif
{
	memset(mUniforms, 0, sizeof(mUniforms));

	if (Context)
	{
		Context.Begin();

		glGenVertexArrays(1, &mStreamVAO);
		glGenBuffers(1, &mStreamVertexBuffer);
		glBindVertexArray(mStreamVAO);
		glBindBuffer(GL_ARRAY_BUFFER, mStreamVertexBuffer);
		VertexBuffer::SetupFlatVAO();
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		mShaderManager = std::make_unique<ShaderManager>();

		CheckError();
		Context.End();
	}
}

RenderDevice::~RenderDevice()
{
	if (Context)
	{
		Context.Begin();
		glDeleteBuffers(1, &mStreamVertexBuffer);
		glDeleteVertexArrays(1, &mStreamVAO);
		mShaderManager->ReleaseResources();
		Context.End();
	}
}

void RenderDevice::SetVertexBuffer(VertexBuffer* buffer)
{
	mVertexBuffer = buffer;
	mNeedApply = true;
}

void RenderDevice::SetIndexBuffer(IndexBuffer* buffer)
{
	mIndexBuffer = buffer;
	mNeedApply = true;
}

void RenderDevice::SetAlphaBlendEnable(bool value)
{
	mAlphaBlend = value;
	mNeedApply = true;
}

void RenderDevice::SetAlphaTestEnable(bool value)
{
	mAlphaTest = value;
	mNeedApply = true;
}

void RenderDevice::SetCullMode(Cull mode)
{
	mCullMode = mode;
	mNeedApply = true;
}

void RenderDevice::SetBlendOperation(BlendOperation op)
{
	mBlendOperation = op;
	mNeedApply = true;
}

void RenderDevice::SetSourceBlend(Blend blend)
{
	mSourceBlend = blend;
	mNeedApply = true;
}

void RenderDevice::SetDestinationBlend(Blend blend)
{
	mDestinationBlend = blend;
	mNeedApply = true;
}

void RenderDevice::SetFillMode(FillMode mode)
{
	mFillMode = mode;
	mNeedApply = true;
}

void RenderDevice::SetMultisampleAntialias(bool value)
{
}

void RenderDevice::SetZEnable(bool value)
{
	mDepthTest = value;
	mNeedApply = true;
}

void RenderDevice::SetZWriteEnable(bool value)
{
	mDepthWrite = value;
	mNeedApply = true;
}

void RenderDevice::SetTexture(Texture* texture)
{
	mTextureUnit.Tex = texture;
	mNeedApply = true;
}

void RenderDevice::SetSamplerFilter(TextureFilter minfilter, TextureFilter magfilter, TextureFilter mipfilter, float maxanisotropy)
{
	mTextureUnit.MinFilter = GetGLMinFilter(minfilter, mipfilter);
	mTextureUnit.MagFilter = (magfilter == TextureFilter::Point || magfilter == TextureFilter::None) ? GL_NEAREST : GL_LINEAR;
	mTextureUnit.MaxAnisotropy = maxanisotropy;
	mNeedApply = true;
}

GLint RenderDevice::GetGLMinFilter(TextureFilter filter, TextureFilter mipfilter)
{
	if (mipfilter == TextureFilter::Linear)
	{
		if (filter == TextureFilter::Point || filter == TextureFilter::None)
			return GL_LINEAR_MIPMAP_NEAREST;
		else
			return GL_LINEAR_MIPMAP_LINEAR;
	}
	else if (mipfilter == TextureFilter::Point)
	{
		if (filter == TextureFilter::Point || filter == TextureFilter::None)
			return GL_NEAREST_MIPMAP_NEAREST;
		else
			return GL_NEAREST_MIPMAP_LINEAR;
	}
	else
	{
		if (filter == TextureFilter::Point || filter == TextureFilter::None)
			return GL_NEAREST;
		else
			return GL_LINEAR;
	}
}

void RenderDevice::SetSamplerState(TextureAddress addressU, TextureAddress addressV, TextureAddress addressW)
{
	mTextureUnit.AddressU = addressU;
	mTextureUnit.AddressV = addressV;
	mTextureUnit.AddressW = addressW;
	mNeedApply = true;
}

void RenderDevice::Draw(PrimitiveType type, int startIndex, int primitiveCount)
{
	static const int modes[] = { GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP };
	static const int toVertexCount[] = { 2, 3, 1 };
	static const int toVertexStart[] = { 0, 0, 2 };

	Context.Begin();
	if (mNeedApply) ApplyChanges();
	glDrawArrays(modes[(int)type], startIndex, toVertexStart[(int)type] + primitiveCount * toVertexCount[(int)type]);
	CheckError();
	Context.End();
}

void RenderDevice::DrawIndexed(PrimitiveType type, int startIndex, int primitiveCount)
{
	static const int modes[] = { GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP };
	static const int toVertexCount[] = { 2, 3, 1 };
	static const int toVertexStart[] = { 0, 0, 2 };

	Context.Begin();
	if (mNeedApply) ApplyChanges();
	glDrawElements(modes[(int)type], toVertexStart[(int)type] + primitiveCount * toVertexCount[(int)type], GL_UNSIGNED_INT, (const void*)(startIndex * sizeof(uint32_t)));
	CheckError();
	Context.End();
}

void RenderDevice::DrawData(PrimitiveType type, int startIndex, int primitiveCount, const void* data)
{
	static const int modes[] = { GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP };
	static const int toVertexCount[] = { 2, 3, 1 };
	static const int toVertexStart[] = { 0, 0, 2 };

	int vertcount = toVertexStart[(int)type] + primitiveCount * toVertexCount[(int)type];

	Context.Begin();
	if (mNeedApply) ApplyChanges();

	glBindBuffer(GL_ARRAY_BUFFER, mStreamVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertcount * (size_t)VertexBuffer::FlatStride, static_cast<const uint8_t*>(data) + startIndex * (size_t)VertexBuffer::FlatStride, GL_STREAM_DRAW);
	glBindVertexArray(mStreamVAO);
	glDrawArrays(modes[(int)type], 0, vertcount);
	CheckError();
	ApplyVertexBuffer();
	CheckError();
	Context.End();
}

void RenderDevice::StartRendering(bool clear, int backcolor, Texture* target, bool usedepthbuffer)
{
	Context.Begin();

	if (target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, target->GetFramebuffer(usedepthbuffer));
		glViewport(0, 0, target->GetWidth(), target->GetHeight());
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, Context.GetWidth(), Context.GetHeight());
	}

	if (clear && usedepthbuffer)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearColor(RPART(backcolor) / 255.0f, GPART(backcolor) / 255.0f, BPART(backcolor) / 255.0f, APART(backcolor) / 255.0f);
		glClearDepthf(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else if (clear)
	{
		glClearColor(RPART(backcolor) / 255.0f, GPART(backcolor) / 255.0f, BPART(backcolor) / 255.0f, APART(backcolor) / 255.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	mNeedApply = true;

	Context.End();
}

void RenderDevice::FinishRendering()
{
}

void RenderDevice::Present()
{
	Context.SwapBuffers();
}

void RenderDevice::ClearTexture(int backcolor, Texture* texture)
{
	StartRendering(true, backcolor, texture, false);
	FinishRendering();
}

void RenderDevice::CopyTexture(Texture* dst, CubeMapFace face)
{
	static const GLenum facegl[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	Context.Begin();
	GLint oldTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &oldTexture);

	glBindTexture(GL_TEXTURE_CUBE_MAP, dst->GetTexture());
	glCopyTexSubImage2D(facegl[(int)face], 0, 0, 0, 0, 0, dst->GetWidth(), dst->GetHeight());
	if (face == CubeMapFace::NegativeZ)
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glBindTexture(GL_TEXTURE_CUBE_MAP, oldTexture);
	Context.End();
}

void RenderDevice::SetVertexBufferData(VertexBuffer* buffer, void* data, int64_t size, VertexFormat format)
{
	Context.Begin();
	CheckError();
	buffer->Format = format;
	GLint oldbinding = 0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldbinding);
	glBindBuffer(GL_ARRAY_BUFFER, buffer->GetBuffer());
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, oldbinding);
	CheckError();
	Context.End();
}

void RenderDevice::SetVertexBufferSubdata(VertexBuffer* buffer, int64_t destOffset, void* data, int64_t size)
{
	Context.Begin();
	CheckError();
	GLint oldbinding = 0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldbinding);
	glBindBuffer(GL_ARRAY_BUFFER, buffer->GetBuffer());
	glBufferSubData(GL_ARRAY_BUFFER, destOffset, size, data);
	glBindBuffer(GL_ARRAY_BUFFER, oldbinding);
	CheckError();
	Context.End();
}

void RenderDevice::SetIndexBufferData(IndexBuffer* buffer, void* data, int64_t size)
{
	Context.Begin();
	CheckError();
	GLint oldbinding = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldbinding);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->GetBuffer());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldbinding);
	CheckError();
	Context.End();
}

void RenderDevice::SetPixels(Texture* texture, const void* data)
{
	texture->SetPixels(data);
	InvalidateTexture(texture);
}

void RenderDevice::SetCubePixels(Texture* texture, CubeMapFace face, const void* data)
{
	texture->SetCubePixels(face, data);
	InvalidateTexture(texture);
}

void RenderDevice::InvalidateTexture(Texture* texture)
{
	if (texture->IsTextureCreated())
	{
		Context.Begin();
		texture->Invalidate();
		Context.End();
		mNeedApply = true;
	}
}

void RenderDevice::CheckError()
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		throw std::runtime_error("OpenGL error!");
}

Shader* RenderDevice::GetActiveShader()
{
	if (mAlphaTest)
		return &mShaderManager->AlphaTestShaders[(int)mShaderName];
	else
		return &mShaderManager->Shaders[(int)mShaderName];
}

void RenderDevice::ApplyChanges()
{
	CheckError();
	ApplyShader();
	CheckError();
	ApplyVertexBuffer();
	CheckError();
	ApplyIndexBuffer();
	CheckError();
	ApplyUniforms();
	CheckError();
	ApplyTextures();
	CheckError();
	ApplyRasterizerState();
	CheckError();
	ApplyBlendState();
	CheckError();
	ApplyDepthState();

	CheckError();

	mNeedApply = false;
}

void RenderDevice::ApplyShader()
{
	glUseProgram(GetActiveShader()->GetProgram());
}

void RenderDevice::ApplyRasterizerState()
{
	if (mCullMode == Cull::None)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}

	GLenum fillMode2GL[] = { GL_FILL, GL_LINE };
	glPolygonMode(GL_FRONT_AND_BACK, fillMode2GL[(int)mFillMode]);
}

void RenderDevice::ApplyBlendState()
{
	if (mAlphaBlend)
	{
		static const GLenum blendOp2GL[] = { GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT };
		static const GLenum blendFunc2GL[] = { GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE };

		glEnable(GL_BLEND);
		glBlendEquation(blendOp2GL[(int)mBlendOperation]);
		glBlendFunc(blendFunc2GL[(int)mSourceBlend], blendFunc2GL[(int)mDestinationBlend]);
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

void RenderDevice::ApplyDepthState()
{
	if (mDepthTest)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(mDepthWrite ? GL_TRUE : GL_FALSE);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}
}

void RenderDevice::ApplyIndexBuffer()
{
	if (mIndexBuffer)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer->GetBuffer());
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

void RenderDevice::ApplyVertexBuffer()
{
	if (mVertexBuffer)
		glBindVertexArray(mVertexBuffer->GetVAO());
}

void RenderDevice::SetShader(ShaderName name)
{
	mShaderName = name;
	mNeedApply = true;
}

static const int uniformLocations[(int)UniformName::NumUniforms] = {
	64, // rendersettings
	0, // transformsettings
	108, // desaturation
	80, // highlightcolor
	16, // worldviewproj
	32, // world
	48, // modelnormal
	68, // FillColor
	72, // vertexColor
	84, // stencilColor
	92, // lightPosAndRadius
	96, // lightOrientation
	100, // light2Radius
	104, // lightColor
	109, // ignoreNormals
	110, // spotLight
	76, // campos,
	112, // texturefactor
};

void RenderDevice::SetUniform(UniformName name, const void* values, int count)
{
	memcpy(&mUniforms[uniformLocations[(int)name]], values, sizeof(float) * count);
	mNeedApply = true;
}

void RenderDevice::ApplyUniforms()
{
	Shader* shader = GetActiveShader();
	auto& locations = shader->UniformLocations;

	glUniformMatrix4fv(locations[(int)UniformName::transformsettings], 1, GL_FALSE, &mUniforms[0].valuef);
	glUniformMatrix4fv(locations[(int)UniformName::worldviewproj], 1, GL_FALSE, &mUniforms[16].valuef);
	glUniformMatrix4fv(locations[(int)UniformName::world], 1, GL_FALSE, &mUniforms[32].valuef);
	glUniformMatrix4fv(locations[(int)UniformName::modelnormal], 1, GL_FALSE, &mUniforms[48].valuef);

	glUniform4fv(locations[(int)UniformName::rendersettings], 1, &mUniforms[64].valuef);
	glUniform4fv(locations[(int)UniformName::FillColor], 1, &mUniforms[68].valuef);
	glUniform4fv(locations[(int)UniformName::vertexColor], 1, &mUniforms[72].valuef);
	glUniform4fv(locations[(int)UniformName::campos], 1, &mUniforms[76].valuef);
	glUniform4fv(locations[(int)UniformName::highlightcolor], 1, &mUniforms[80].valuef);
	glUniform4fv(locations[(int)UniformName::stencilColor], 1, &mUniforms[84].valuef);
	glUniform4fv(locations[(int)UniformName::lightColor], 1, &mUniforms[88].valuef);
	glUniform4fv(locations[(int)UniformName::lightPosAndRadius], 1, &mUniforms[92].valuef);
	glUniform3fv(locations[(int)UniformName::lightOrientation], 1, &mUniforms[96].valuef);
	glUniform2fv(locations[(int)UniformName::light2Radius], 1, &mUniforms[100].valuef);
	glUniform4fv(locations[(int)UniformName::lightColor], 1, &mUniforms[104].valuef);

	glUniform1fv(locations[(int)UniformName::desaturation], 1, &mUniforms[108].valuef);
	glUniform1fv(locations[(int)UniformName::ignoreNormals], 1, &mUniforms[109].valuef);
	glUniform1fv(locations[(int)UniformName::spotLight], 1, &mUniforms[110].valuef);

	glUniform4fv(locations[(int)UniformName::texturefactor], 1, &mUniforms[112].valuef);

	for (int i = 0; i < Shader::MaxSamplers; i++)
		glUniform1i(shader->SamplerLocations[i], i);
}

void RenderDevice::ApplyTextures()
{
	static const int wrapMode[] = { GL_REPEAT, GL_CLAMP_TO_EDGE };

	glActiveTexture(GL_TEXTURE0);
	if (mTextureUnit.Tex)
	{
		GLenum target = mTextureUnit.Tex->IsCubeTexture() ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

		glBindTexture(target, mTextureUnit.Tex->GetTexture());
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mTextureUnit.MinFilter);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mTextureUnit.MagFilter);
		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapMode[(int)mTextureUnit.AddressU]);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapMode[(int)mTextureUnit.AddressV]);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, wrapMode[(int)mTextureUnit.AddressW]);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

/////////////////////////////////////////////////////////////////////////////

RenderDevice* RenderDevice_New(void* hwnd)
{
	RenderDevice *device = new RenderDevice(hwnd);
	if (!device->Context)
	{
		delete device;
		return nullptr;
	}
	else
	{
		return device;
	}
}

void RenderDevice_Delete(RenderDevice* device)
{
	delete device;
}

void RenderDevice_SetShader(RenderDevice* device, ShaderName name)
{
	device->SetShader(name);
}

void RenderDevice_SetUniform(RenderDevice* device, UniformName name, const void* values, int count)
{
	device->SetUniform(name, values, count);
}

void RenderDevice_SetVertexBuffer(RenderDevice* device, VertexBuffer* buffer)
{
	device->SetVertexBuffer(buffer);
}

void RenderDevice_SetIndexBuffer(RenderDevice* device, IndexBuffer* buffer)
{
	device->SetIndexBuffer(buffer);
}

void RenderDevice_SetAlphaBlendEnable(RenderDevice* device, bool value)
{
	device->SetAlphaBlendEnable(value);
}

void RenderDevice_SetAlphaTestEnable(RenderDevice* device, bool value)
{
	device->SetAlphaTestEnable(value);
}

void RenderDevice_SetCullMode(RenderDevice* device, Cull mode)
{
	device->SetCullMode(mode);
}

void RenderDevice_SetBlendOperation(RenderDevice* device, BlendOperation op)
{
	device->SetBlendOperation(op);
}

void RenderDevice_SetSourceBlend(RenderDevice* device, Blend blend)
{
	device->SetSourceBlend(blend);
}

void RenderDevice_SetDestinationBlend(RenderDevice* device, Blend blend)
{
	device->SetDestinationBlend(blend);
}

void RenderDevice_SetFillMode(RenderDevice* device, FillMode mode)
{
	device->SetFillMode(mode);
}

void RenderDevice_SetMultisampleAntialias(RenderDevice* device, bool value)
{
	device->SetMultisampleAntialias(value);
}

void RenderDevice_SetZEnable(RenderDevice* device, bool value)
{
	device->SetZEnable(value);
}

void RenderDevice_SetZWriteEnable(RenderDevice* device, bool value)
{
	device->SetZWriteEnable(value);
}

void RenderDevice_SetTexture(RenderDevice* device, Texture* texture)
{
	device->SetTexture(texture);
}

void RenderDevice_SetSamplerFilter(RenderDevice* device, TextureFilter minfilter, TextureFilter magfilter, TextureFilter mipfilter, float maxanisotropy)
{
	device->SetSamplerFilter(minfilter, magfilter, mipfilter, maxanisotropy);
}

void RenderDevice_SetSamplerState(RenderDevice* device, TextureAddress addressU, TextureAddress addressV, TextureAddress addressW)
{
	device->SetSamplerState(addressU, addressV, addressW);
}

void RenderDevice_Draw(RenderDevice* device, PrimitiveType type, int startIndex, int primitiveCount)
{
	device->Draw(type, startIndex, primitiveCount);
}

void RenderDevice_DrawIndexed(RenderDevice* device, PrimitiveType type, int startIndex, int primitiveCount)
{
	device->DrawIndexed(type, startIndex, primitiveCount);
}

void RenderDevice_DrawData(RenderDevice* device, PrimitiveType type, int startIndex, int primitiveCount, const void* data)
{
	device->DrawData(type, startIndex, primitiveCount, data);
}

void RenderDevice_StartRendering(RenderDevice* device, bool clear, int backcolor, Texture* target, bool usedepthbuffer)
{
	device->StartRendering(clear, backcolor, target, usedepthbuffer);
}

void RenderDevice_FinishRendering(RenderDevice* device)
{
	device->FinishRendering();
}

void RenderDevice_Present(RenderDevice* device)
{
	device->Present();
}

void RenderDevice_ClearTexture(RenderDevice* device, int backcolor, Texture* texture)
{
	device->ClearTexture(backcolor, texture);
}

void RenderDevice_CopyTexture(RenderDevice* device, Texture* dst, CubeMapFace face)
{
	device->CopyTexture(dst, face);
}

void RenderDevice_SetVertexBufferData(RenderDevice* device, VertexBuffer* buffer, void* data, int64_t size, VertexFormat format)
{
	device->SetVertexBufferData(buffer, data, size, format);
}

void RenderDevice_SetVertexBufferSubdata(RenderDevice* device, VertexBuffer* buffer, int64_t destOffset, void* data, int64_t size)
{
	device->SetVertexBufferSubdata(buffer, destOffset, data, size);
}

void RenderDevice_SetIndexBufferData(RenderDevice* device, IndexBuffer* buffer, void* data, int64_t size)
{
	device->SetIndexBufferData(buffer, data, size);
}

void RenderDevice_SetPixels(RenderDevice* device, Texture* texture, const void* data)
{
	device->SetPixels(texture, data);
}

void RenderDevice_SetCubePixels(RenderDevice* device, Texture* texture, CubeMapFace face, const void* data)
{
	device->SetCubePixels(texture, face, data);
}
