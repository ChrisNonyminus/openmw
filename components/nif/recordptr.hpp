#ifndef OPENMW_COMPONENTS_NIF_RECORDPTR_HPP
#define OPENMW_COMPONENTS_NIF_RECORDPTR_HPP

#include "niffile.hpp"
#include "nifstream.hpp"
#include <vector>

namespace Nif
{

    /** A reference to another record. It is read as an index from the
        NIF, and later looked up in the index table to get an actual
        pointer.
    */
    template <class X>
    class RecordPtrT
    {
        union
        {
            intptr_t index;
            X* ptr;
        };

    public:
        RecordPtrT()
            : index(-2)
        {
        }

        RecordPtrT(X* ptr)
            : ptr(ptr)
        {
        }

        /// Read the index from the nif
        void read(NIFStream* nif)
        {
            // Can only read the index once
            assert(index == -2);

            // Store the index for later
            index = nif->getInt();
            assert(index >= -1);
        }

        /// Resolve index to pointer
        void post(Reader& nif)
        {
            if (index < 0)
                ptr = nullptr;
            else
            {
                Record* r = nif.getRecord(index);
                // And cast it
                ptr = dynamic_cast<X*>(r);
                assert(ptr != nullptr);
            }
        }

        /// Look up the actual object from the index
        const X* getPtr() const
        {
            assert(ptr != nullptr);
            return ptr;
        }
        X* getPtr()
        {
            assert(ptr != nullptr);
            return ptr;
        }

        const X& get() const { return *getPtr(); }
        X& get() { return *getPtr(); }

        /// Syntactic sugar
        const X* operator->() const { return getPtr(); }
        X* operator->() { return getPtr(); }

        /// Pointers are allowed to be empty
        bool empty() const { return ptr == nullptr; }
    };

    /** A list of references to other records. These are read as a list,
        and later converted to pointers as needed. Not an optimized
        implementation.
     */
    template <class X>
    using RecordListT = std::vector<RecordPtrT<X>>;

    template <class T>
    void readRecordList(NIFStream* nif, RecordListT<T>& list)
    {
        const int length = nif->getInt();

        if (length < 0)
            throw std::runtime_error("Negative NIF record list length: " + std::to_string(length));

        list.resize(static_cast<std::size_t>(length));

        for (auto& value : list)
            value.read(nif);
    }

    template <class T>
    void postRecordList(Reader& nif, RecordListT<T>& list)
    {
        for (auto& value : list)
            value.post(nif);
    }

    struct Node;
    struct Extra;
    struct Property;
    struct NiUVData;
    struct NiPosData;
    struct NiVisData;
    struct Controller;
    struct Named;
    struct NiSkinData;
    struct NiFloatData;
    struct NiMorphData;
    struct NiPixelData;
    struct NiColorData;
    struct NiKeyframeData;
    struct NiTriStripsData;
    struct NiSkinInstance;
    struct NiSourceTexture;
    struct NiPalette;
    struct NiParticleModifier;
    struct NiBoolData;
    struct NiSkinPartition;
    struct BSShaderTextureSet;
    struct NiGeometryData;
    struct BSShaderProperty;
    struct NiAlphaProperty;
    struct NiCollisionObject;
    struct bhkWorldObject;
    struct bhkShape;
    struct bhkSerializable;
    struct hkPackedNiTriStripsData;
    struct NiAccumulator;
    struct NiInterpolator;
    struct NiStringPalette;
    struct NiControllerManager;
    struct NiBlendInterpolator;
    struct NiDefaultAVObjectPalette;
    struct NiControllerSequence;

    using NodePtr = RecordPtrT<Node>;
    using ExtraPtr = RecordPtrT<Extra>;
    using NiUVDataPtr = RecordPtrT<NiUVData>;
    using NiPosDataPtr = RecordPtrT<NiPosData>;
    using NiVisDataPtr = RecordPtrT<NiVisData>;
    using ControllerPtr = RecordPtrT<Controller>;
    using NamedPtr = RecordPtrT<Named>;
    using NiSkinDataPtr = RecordPtrT<NiSkinData>;
    using NiMorphDataPtr = RecordPtrT<NiMorphData>;
    using NiPixelDataPtr = RecordPtrT<NiPixelData>;
    using NiFloatDataPtr = RecordPtrT<NiFloatData>;
    using NiColorDataPtr = RecordPtrT<NiColorData>;
    using NiKeyframeDataPtr = RecordPtrT<NiKeyframeData>;
    using NiSkinInstancePtr = RecordPtrT<NiSkinInstance>;
    using NiSourceTexturePtr = RecordPtrT<NiSourceTexture>;
    using NiPalettePtr = RecordPtrT<NiPalette>;
    using NiParticleModifierPtr = RecordPtrT<NiParticleModifier>;
    using NiBoolDataPtr = RecordPtrT<NiBoolData>;
    using NiSkinPartitionPtr = RecordPtrT<NiSkinPartition>;
    using BSShaderTextureSetPtr = RecordPtrT<BSShaderTextureSet>;
    using NiGeometryDataPtr = RecordPtrT<NiGeometryData>;
    using BSShaderPropertyPtr = RecordPtrT<BSShaderProperty>;
    using NiAlphaPropertyPtr = RecordPtrT<NiAlphaProperty>;
    using NiCollisionObjectPtr = RecordPtrT<NiCollisionObject>;
    using bhkWorldObjectPtr = RecordPtrT<bhkWorldObject>;
    using bhkShapePtr = RecordPtrT<bhkShape>;
    using hkPackedNiTriStripsDataPtr = RecordPtrT<hkPackedNiTriStripsData>;
    using NiAccumulatorPtr = RecordPtrT<NiAccumulator>;
    using NiInterpolatorPtr = RecordPtrT<NiInterpolator>;
    using NiStringPalettePtr = RecordPtrT<NiStringPalette>;
    using NiControllerManagerPtr = RecordPtrT<NiControllerManager>;
    using NiBlendInterpolatorPtr = RecordPtrT<NiBlendInterpolator>;
    using NiDefaultAVObjectPalettePtr = RecordPtrT<NiDefaultAVObjectPalette>;

    using NodeList = RecordListT<Node>;
    using PropertyList = RecordListT<Property>;
    using ExtraList = RecordListT<Extra>;
    using NiSourceTextureList = RecordListT<NiSourceTexture>;
    using NiInterpolatorList = RecordListT<NiInterpolator>;
    using NiTriStripsDataList = RecordListT<NiTriStripsData>;
    using bhkShapeList = RecordListT<bhkShape>;
    using bhkSerializableList = RecordListT<bhkSerializable>;
    using NiControllerSequenceList = RecordListT<NiControllerSequence>;

} // Namespace
#endif
